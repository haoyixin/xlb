#ifndef XLB_SCHEDULER_H
#define XLB_SCHEDULER_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "utils/common.h"
#include "utils/time.h"

namespace xlb {

class Task;
class Context;

// TODO: histogram with more metrics
struct sched_stats {
  uint64_t cnt_idle;
  uint64_t cycles_idle;
};

// The non-instantiable base class for schedulers.  Implements common routines
// needed for scheduling.
class Scheduler {
public:
  virtual ~Scheduler() = default;

  // Runs the scheduler loop forever.
  void ScheduleLoop();

protected:
  explicit Scheduler() : stats_(), checkpoint_(), ns_per_cycle_(1e9 / tsc_hz) {}

  virtual Task *Next() = 0;

  // Runs the scheduler once, return false for idle loop.
  virtual bool ScheduleOnce(Context *ctx) = 0;

private:
  struct sched_stats stats_;

  uint64_t checkpoint_;

  double ns_per_cycle_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

// The default scheduler, which round robins the tasks to run
// TODO: weighted round robin
class DefaultScheduler : public Scheduler {
public:
  explicit DefaultScheduler();

  ~DefaultScheduler() override = default;

  bool ScheduleOnce(Context *ctx) override;

  Task *Next() override;

private:
  std::vector<Task> tasks_;
  std::vector<Task>::iterator pos_;
};

} // namespace xlb

#endif // XLB_SCHEDULER_H
