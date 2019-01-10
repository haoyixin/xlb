#ifndef XLB_TASK_H
#define XLB_TASK_H

#include "utils/cuckoo_map.h"
#include "utils/unsafe_pool.h"

#include "packet_batch.h"
#include "config.h"
#include "types.h"

namespace xlb {

class Module;
struct Context;

// Functor used by a Worker's Scheduler to run a task in a module.
class Task {
public:
  static const size_t kBatchPoolSize = 512;
  using BatchPool = utils::UnsafePool<PacketBatch>;

  Task() = default;

  Task(TaskFunc task_func)
      : task_func_(task_func), batch_pool_(kBatchPoolSize, CONFIG.nic.socket) {
    dead_batch_.clear();
  }

  ~Task() = default;

  TaskResult Run(Context *ctx) const {
    return task_func_(ctx, AllocBatch().get());
  }

  // RVO maybe has be done by compiler
  BatchPool::Shared AllocBatch() const { return batch_pool_.GetShared(); }

  void DropPacket(Packet *pkt) {
    dead_batch_.add(pkt);
    if (dead_batch_.cnt() > Packet::kMaxBurst)
      dead_batch_.Free();
  }

private:
  // Used by Run()
  TaskFunc task_func_;

  // A packet batch for storing packets to free
  mutable PacketBatch dead_batch_;

  mutable BatchPool batch_pool_;
};

} // namespace xlb

#endif // XLB_TASK_H
