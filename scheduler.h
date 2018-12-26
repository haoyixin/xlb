#ifndef XLB_SCHEDULER_H
#define XLB_SCHEDULER_H

#include "context.h"
// Task::Run() is defined in module.h
#include "module.h"
#include "task.h"
#include "worker.h"

#include "utils/common.h"
#include "utils/time.h"

#include <vector>

namespace xlb {

class Task;
class Context;

// TODO: histogram with more metrics

// The non-instantiable base class for schedulers.  Implements common routines
// needed for scheduling.
class Scheduler {
public:
  struct Stats {
    uint64_t cycles_idle;
    uint64_t cycles_busy;
  };

  struct Usage {
    // this is 5 seconds by default
    // TODO: maybe this should be in CONFIG
    static const uint64_t kDefaultInterval = 5e9;
    double ratio;
    uint64_t checkpoint;
  };

  virtual ~Scheduler() = default;

  // Runs the scheduler loop forever.
  void ScheduleLoop() {
    Context ctx(Worker::current());
    bool idle = false;
    uint64_t cycles = 0;
    checkpoint_ = usage_.checkpoint = ctx.worker()->current_tsc();

    // The main scheduling, running, accounting loop.
    for (uint64_t round = 0;; ++round) {
      if (ctx.worker()->state() == Worker::State::QUITTING)
        break;

      ctx.reset(Next());
      cycles = ctx.worker()->current_tsc() - checkpoint_;

      if (idle)
        stats_.cycles_idle += cycles;
      else
        stats_.cycles_busy += cycles;

      checkpoint_ = ctx.worker()->current_tsc();
      if (checkpoint_ - usage_.checkpoint > Usage::kDefaultInterval) {
        usage_.ratio = double(stats_.cycles_busy) /
                       (stats_.cycles_busy + stats_.cycles_idle);
        usage_.checkpoint = checkpoint_;

        stats_.cycles_busy = 0;
        stats_.cycles_idle = 0;
      }

      idle = !ScheduleOnce(&ctx);
    }
  }

  double usage_ratio() { return usage_.ratio; }

protected:
  explicit Scheduler() : stats_(), usage_(), checkpoint_() {}

  virtual Task *Next() = 0;

  // Runs the scheduler once, return false for idle loop.
  // As we only hava a DefaultSchedular, speculative devirtualization in gcc
  // maybe work
  virtual bool ScheduleOnce(Context *ctx) = 0;

private:
  Stats stats_;
  Usage usage_;

  uint64_t checkpoint_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

// The default scheduler, which round robins the tasks to run
// TODO: weighted round robin
class DefaultScheduler : public Scheduler {
public:
  explicit DefaultScheduler() : Scheduler() {
    for (auto &t : *Task::protos())
      tasks_.emplace_back(t.second.Clone());

    CHECK_GT(tasks_.size(), 0);

    pos_ = tasks_.begin();
  }

  ~DefaultScheduler() override = default;

  bool ScheduleOnce(Context *ctx) override {
    return ctx->task()->Run(ctx).packets != 0;
  }

  Task *Next() override {
    if (pos_ == tasks_.end())
      pos_ = tasks_.begin();

    return &*pos_++;
  }

private:
  std::vector<Task> tasks_;
  std::vector<Task>::iterator pos_;
};

} // namespace xlb

#endif // XLB_SCHEDULER_H
