#ifndef XLB_TASK_H
#define XLB_TASK_H

#include "packet_batch.h"
#include <utils/cuckoo_map.h>

#define MAX_PBATCH_CNT 256

namespace xlb {

class Module;
struct Context;

struct task_result {
  uint32_t packets;
  uint64_t bits;
};

class TaskProto;

// Functor used by a Worker's Scheduler to run a task in a module.
class Task {
public:
  Task() = default;
  // When this task is scheduled it will execute 'm' with 'arg'.
  Task(Module *m, void *arg)
      : module_(m), arg_(arg), pbatch_idx_(),
        pbatch_(new PacketBatch[MAX_PBATCH_CNT]) {
    dead_batch_.clear();
  }

  ~Task() { delete[] pbatch_; }

  // Do not track used/unsued for efficiency
  PacketBatch *AllocPacketBatch() const {
    DCHECK_LT(pbatch_idx_, MAX_PBATCH_CNT);
    PacketBatch *batch = &pbatch_[pbatch_idx_++];
    batch->clear();
    return batch;
  }

  void ClearPacketBatch() const { pbatch_idx_ = 0; }

  Module *module() const { return module_; }

  PacketBatch *dead_batch() const { return &dead_batch_; }

  struct task_result operator()(Context *ctx) const;

  static utils::CuckooMap<std::string, TaskProto> *TaskProtos();

private:
  // Used by operator().
  Module *module_;
  // Auxiliary value passed to Module::RunTask().
  void *arg_;

  // A packet batch for storing packets to free
  mutable PacketBatch dead_batch_;

  // Simple packet batch pool
  mutable int pbatch_idx_;
  mutable PacketBatch *pbatch_;

  static utils::CuckooMap<std::string, TaskProto> *task_protos_;
};

class TaskProto {
public:
  TaskProto() = default;
  TaskProto(Module *m, void *arg) : modules_(m), arg_(arg) {}
  Task Clone() { return Task(modules_, arg_); }

private:
  Module *modules_;
  void *arg_;
};

} // namespace xlb

#endif // XLB_TASK_H
