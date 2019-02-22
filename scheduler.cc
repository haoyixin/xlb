#include "scheduler.h"

#include <utility>

#include <glog/logging.h>

#include "module.h"

namespace xlb {

namespace {

double get_cpu_usage(void *arg) { return Scheduler::CpuUsage(); }
bvar::PassiveStatus<double> cpu_usage("xlb_scheduler", "wrr_cpu_usage",
                                      get_cpu_usage, nullptr);
}  // namespace

Scheduler::Scheduler()
    : checkpoint_(), runnable_(new TaskQueue()), blocked_(new TaskQueue()) {}

Scheduler::~Scheduler() {
  for (auto &t : runnable_->container())
    delete(t);

  for (auto &t : blocked_->container())
    delete(t);

  delete(runnable_);
  delete(blocked_);
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

void Scheduler::Loop() {
  bool idle{false};
  Task::Context *ctx;
  uint64_t cycles{};

  auto worker = Worker::current();
  //  checkpoint_ = usage_.checkpoint = worker->current_tsc();
  checkpoint_ = worker->current_tsc();

  for (auto &m : Modules::instance()) m->InitInWorker(worker->id());
  for (auto &t : runnable_->container()) t->context_.worker_ = worker;

  // The main scheduling, running, accounting loop.
  for (uint64_t round = 0;; ++round) {
    if (Worker::aborting()) break;

    ctx = next_ctx();

    ctx->worker_->UpdateTsc();
    ctx->worker_->IncrSilentDrops(ctx->silent_drops_);
    ctx->silent_drops_ = 0;

    cycles = ctx->worker_->current_tsc() - checkpoint_;

    if (idle)
      M::Adder<TS("idle_cycles")>() << cycles;
    else
      M::Adder<TS("busy_cycles")>() << cycles;

    checkpoint_ = ctx->worker_->current_tsc();

    idle = (ctx->task_->func_(ctx).packets != 0);
  }
}

double Scheduler::CpuUsage() {
  auto idle = &M::PerSecond<TS("idle_cycles")>();
  auto busy = &M::PerSecond<TS("busy_cycles")>();

  // TODO: add a patch to brpc/bvar for fixing this
  double idle_ps = idle->get_value(idle->window_size());
  double busy_ps = busy->get_value(busy->window_size());

  return busy_ps / (busy_ps + idle_ps);
}

Scheduler::Task::Task(Func &&func, uint8_t weight)
    : func_(std::move(func)),
      context_(),
      max_weight_(weight),
      current_weight_(weight) {
  CHECK_NE(weight, 0);
  context_.dead_batch_.Clear();
  context_.stage_batch_.Clear();
  context_.task_ = this;
}

Scheduler::Task::~Task() {
  context_.dead_batch_.Free();
  context_.stage_batch_.Free();
}

void Scheduler::Task::Context::DropPacket(Packet *pkt) {
  if (dead_batch_.Full()) dead_batch_.Free();

  dead_batch_.Push(pkt);
}

void Scheduler::Task::Context::HoldPacket(Packet *pkt) {
  if (!stage_batch_.Full())
    stage_batch_.Push(pkt);
  else
    DropPacket(pkt);
}

}  // namespace xlb
