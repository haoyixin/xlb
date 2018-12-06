#include "scheduler.h"

#include "module.h"
#include "task.h"
#include "worker.h"

namespace xlb {

void Scheduler::ScheduleLoop() {
  uint64_t now = rdtsc();

  this->checkpoint_ = now;
  Worker::Current()->set_silent_drops(0);

  Context ctx = {};
  ctx.wid = Worker::Current()->Core();

  // The main scheduling, running, accounting loop.
  for (uint64_t round = 0;; ++round) {
    if (Worker::Current()->State() == WORKER_QUITTING)
      break;

    ctx.task = Next();
    ctx.current_tsc = this->checkpoint_; // Tasks see updated tsc.
    ctx.current_ns = this->checkpoint_ * this->ns_per_cycle_;
    ctx.silent_drops = 0;

    Worker::Current()->set_current_tsc(ctx.current_tsc);
    Worker::Current()->set_current_ns(ctx.current_ns);

    auto idle = !ScheduleOnce(&ctx);

    Worker::Current()->incr_silent_drops(ctx.silent_drops);

    now = rdtsc();

    if (idle)
      this->stats_.cnt_idle += 1;
    this->stats_.cycles_idle += (now - this->checkpoint_);

    this->checkpoint_ = now;
  }
}

bool DefaultScheduler::ScheduleOnce(Context *ctx) {
  // Run.
  return (*ctx->task)(ctx).packets != 0;
}

Task *DefaultScheduler::Next() {
  if (pos_ == tasks_.end())
    pos_ = tasks_.begin();

  return &*pos_++;
}

DefaultScheduler::DefaultScheduler() : Scheduler() {
  for (auto &t : *Task::TaskProtos())
    tasks_.emplace_back(t.second.Clone());

  CHECK_GT(tasks_.size(), 0);

  pos_ = tasks_.begin();
}

} // namespace xlb
