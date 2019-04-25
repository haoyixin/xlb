#include "runtime/scheduler.h"
#include "runtime/module.h"

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

double get_cpu_usage_trivial(void *arg) {
  return Scheduler::CpuUsage<TS("idle_cycles_trivial"),
                             TS("busy_cycles_trivial")>();
}

bvar::PassiveStatus<double> cpu_usage_master("xlb_scheduler",
                                             "cpu_usage_master",
                                             get_cpu_usage_master, nullptr);
bvar::PassiveStatus<double> cpu_usage_slave("xlb_scheduler", "cpu_usage_slaves",
                                            get_cpu_usage_slaves, nullptr);
bvar::PassiveStatus<double> cpu_usage_trivial("xlb_scheduler",
                                              "cpu_usage_trivial",
                                              get_cpu_usage_trivial, nullptr);
}  // namespace

Scheduler::Scheduler() : runnable_(ALLOC), checkpoint_() {}

/*
Scheduler::~Scheduler() {
for (auto &t : runnable_) delete (t);
for (auto &t : blocked_->container()) delete (t);

delete (runnable_);
delete (blocked_);
}
 */

Scheduler::Task::Context *Scheduler::next_ctx() {
  /*
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
   */
  Task *best = nullptr;
  size_t total = 0;

  for (auto &task : runnable_) {
    task->current_weight_ += task->max_weight_;
    total += task->max_weight_;

    if (!best || task->current_weight_ > best->current_weight_) best = task;
  }

  DCHECK_NOTNULL(best);

  best->current_weight_ -= total;
  return &best->context_;
}

Scheduler::Task::Task(Func &&func, uint32_t weight)
    : func_(std::move(func)),
      current_weight_(0),
      max_weight_(weight),
      context_() {
  CHECK_GT(weight, 0);
  context_.dead_batch_.Clear();
  context_.stage_batch_.Clear();
  context_.task_ = this;
}

Scheduler::Task::~Task() {
  if (!context_.dead_batch_.Empty()) context_.dead_batch_.Free();
  if (!context_.stage_batch_.Empty()) context_.stage_batch_.Free();
}

void Scheduler::Task::Context::Drop(Packet *pkt) {
  if (dead_batch_.Full()) {
    silent_drops_ += dead_batch_.cnt();
    dead_batch_.Free();
    dead_batch_.Clear();
  }

  dead_batch_.Push(pkt);
}

void Scheduler::Task::Context::Hold(Packet *pkt) { stage_batch_.Push(pkt); }

void Scheduler::Task::Context::Hold(PacketBatch *pkts) {
  stage_batch_.Push(pkts);
}

}  // namespace xlb
