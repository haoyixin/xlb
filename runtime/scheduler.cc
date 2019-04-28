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

void Scheduler::RegisterTask(Task::Func &&func, std::string_view name) {
  auto *task = new Task(std::move(func), name);
  CHECK_EQ(
      task->display_weight_.expose_as(
          "xlb_scheduler", utils::Format("task_%s_%d_%s_weight", W_TYPE_STR,
                                         W_ID, task->name_.c_str())),
      0);
  runnable_.emplace_back(task);
}

Scheduler::Task::Context *Scheduler::next_ctx() {
  // This is a variant of nginx's smooth weighted round-robin algorithm. We
  // update the effective weights according to the load. See 'update_weight' for
  // details
  Task *best = nullptr;
  size_t total = 0;

  for (auto &task : runnable_) {
    task->current_weight_ += task->effective_weight_;
    total += task->effective_weight_;

    if (!best || task->current_weight_ > best->current_weight_) best = task;
  }

  DCHECK_NOTNULL(best);

  best->current_weight_ -= total;
  return &best->context_;
}

void Scheduler::Task::update_weight(bool idle, uint64_t cycles) {
  int64_t expect_weight;

  // It is almost impossible to overflow here
  if (idle)
    expect_weight = effective_weight_ - cycles;
  else
    expect_weight = effective_weight_ + cycles;

  if (expect_weight <= min_weight_)
    effective_weight_ = min_weight_;
  else if (expect_weight >= max_weight_)
    effective_weight_ = max_weight_;
  else
    effective_weight_ = expect_weight;

  display_weight_.set_value(effective_weight_);
}

Scheduler::Task::Task(Func &&func, std::string_view name)
    : func_(std::move(func)),
      name_(name),
      min_weight_(kMinWeight * utils::tsc_us),
      max_weight_(kMaxWeight * utils::tsc_us),
      current_weight_(min_weight_),
      effective_weight_(min_weight_),
      display_weight_(effective_weight_),
      context_() {
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
