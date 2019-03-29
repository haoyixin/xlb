#include "scheduler.h"

#include <utility>

#include <glog/logging.h>

#include "module.h"

#include "utils/common.h"
#include "utils/allocator.h"

namespace xlb {

namespace {

double get_cpu_usage_slaves(void *arg) {
  return Scheduler::CpuUsage<TS("idle_cycles_slaves"),
                             TS("busy_cycles_slaves")>();
}

double get_cpu_usage_master(void *arg) {
  return Scheduler::CpuUsage<TS("idle_cycles_master"),
                             TS("busy_cycles_master")>();
}

bvar::PassiveStatus<double> cpu_usage_master("xlb_scheduler",
                                             "cpu_usage_master",
                                             get_cpu_usage_master, nullptr);
bvar::PassiveStatus<double> cpu_usage_slave("xlb_scheduler", "cpu_usage_slaves",
                                            get_cpu_usage_slaves, nullptr);
}  // namespace

Scheduler::Scheduler()
    : runnable_(new TaskQueue()), blocked_(new TaskQueue()), checkpoint_() {}

Scheduler::~Scheduler() {
  for (auto &t : runnable_->container()) delete (t);

  for (auto &t : blocked_->container()) delete (t);

  delete (runnable_);
  delete (blocked_);
}

Scheduler::Task::Context *Scheduler::next_ctx() {
  while (runnable_->empty()) std::swap(runnable_, blocked_);

  Task *t;

  if (--(t = runnable_->top())->current_weight_ <= 0) {
    t->current_weight_ = t->max_weight_;
    blocked_->push(t);
    runnable_->pop();
  } else {
    runnable_->decrease_key_top();
  }

  return &t->context_;
}

void Scheduler::MasterLoop() {
  bool idle{false};
  Task::Context *ctx;
  uint64_t cycles{};

  auto worker = Worker::current();
  checkpoint_ = worker->current_tsc();

  for (auto &m : Modules::instance()) m->InitInMaster();
  worker->confirm_master();

  for (auto &t : runnable_->container()) t->context_.worker_ = worker;

  // The main scheduling, running, accounting master loop.
  for (uint64_t round = 0;; ++round) {
    if (worker->slaves_aborted()) break;

    ctx = next_ctx();

    ctx->worker_->UpdateTsc();
    ctx->worker_->IncrSilentDrops(ctx->silent_drops_);
    ctx->silent_drops_ = 0;

    cycles = ctx->worker_->current_tsc() - checkpoint_;

    if (idle)
      M::Adder<TS("idle_cycles_master")>() << cycles;
    else
      M::Adder<TS("busy_cycles_master")>() << cycles;

    checkpoint_ = ctx->worker_->current_tsc();

    idle = (ctx->task_->func_(ctx).packets != 0);
  }
}

void Scheduler::SlaveLoop() {
  bool idle{false};
  Task::Context *ctx;
  uint64_t cycles{};

  auto worker = Worker::current();
  checkpoint_ = worker->current_tsc();

  for (auto &m : Modules::instance()) m->InitInSlave(worker->id());

  for (auto &t : runnable_->container()) t->context_.worker_ = worker;

  // The main scheduling, running, accounting slave loop.
  for (uint64_t round = 0;; ++round) {
    if (worker->aborting()) break;

    ctx = next_ctx();

    ctx->worker_->UpdateTsc();
    ctx->worker_->IncrSilentDrops(ctx->silent_drops_);
    ctx->silent_drops_ = 0;

    cycles = ctx->worker_->current_tsc() - checkpoint_;

    if (idle)
      M::Adder<TS("idle_cycles_slaves")>() << cycles;
    else
      M::Adder<TS("busy_cycles_slaves")>() << cycles;

    checkpoint_ = ctx->worker_->current_tsc();

    idle = (ctx->task_->func_(ctx).packets != 0);
  }
}

Scheduler::Task::Task(Func &&func, uint8_t weight)
    : func_(std::move(func)),
      context_(),
      current_weight_(weight),
      max_weight_(weight) {
  CHECK_NE(weight, 0);
  context_.dead_batch_.Clear();
  context_.stage_batch_.Clear();
  context_.task_ = this;
}

Scheduler::Task::~Task() {
  context_.dead_batch_.Free();
  context_.stage_batch_.Free();
}

void Scheduler::Task::Context::Drop(Packet *pkt) {
  if (dead_batch_.Full()) dead_batch_.Free();

  dead_batch_.Push(pkt);
}

void Scheduler::Task::Context::Hold(Packet *pkt) {
    stage_batch_.Push(pkt);
}

void Scheduler::Task::Context::Hold(PacketBatch *pkts) {
  stage_batch_.Push(pkts);
}

}  // namespace xlb
