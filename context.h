#ifndef XLB_CONTEXT_H
#define XLB_CONTEXT_H

#include "packet.h"
#include "worker.h"

namespace xlb {

class Task;

class Context {
public:
  explicit Context(Worker *worker) : worker_(worker) {}

  void reset(Task *task) {
    task_ = task;
    worker_->update_tsc();
    worker_->incr_silent_drops(silent_drops_);
    silent_drops_ = 0;
  }

  Worker *worker() { return worker_; }

  Task *task() { return task_; }
  void incr_silent_drops(uint64_t drops) { silent_drops_ += drops; }

private:
  Task *task_;
  Worker *worker_;

  // Set by module scheduler, read by a task scheduler
  uint64_t silent_drops_;
};

} // namespace xlb

#endif // XLB_CONTEXT_H
