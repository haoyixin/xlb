#ifndef XLB_TASK_H
#define XLB_TASK_H

#include "config.h"
#include "packet_batch.h"

#include "utils/unsafe_pool.h"
#include "utils/cuckoo_map.h"

namespace xlb {

class Module;
struct Context;

// Functor used by a Worker's Scheduler to run a task in a module.
class Task {
public:
  static const size_t kBatchPoolSize = 512;
  using BatchPool = utils::UnsafePool<PacketBatch>;

  struct Result {
    uint64_t packets;
  };

  class Proto {
  public:
    Proto() = default;
    Proto(Module *m, void *arg) : modules_(m), arg_(arg) {}
    Task Clone() { return Task(modules_, arg_); }

  private:
    Module *modules_;
    void *arg_;
  };

  using ProtoMap = utils::CuckooMap<std::string, Proto>;

  Task() = default;
  // When this task is scheduled it will execute 'm' with 'arg'.
  Task(Module *m, void *arg)
      : module_(m), arg_(arg), batch_pool_(kBatchPoolSize, CONFIG.nic.socket) {
    dead_batch_.clear();
  }

  ~Task() = default;

  // This func is define in module.h for decyclization
  inline Result Run(Context *ctx) const;

  // RVO maybe has be done by compiler, you should not use it outside stack
  auto AllocBatch() const { return batch_pool()->get_shared(); }

  void DropPacket(Packet *pkt) {
    dead_batch()->add(pkt);
    if (dead_batch()->cnt() > Packet::kMaxBurst)
      dead_batch()->Free();
  }

  Module *module() const { return module_; }
  PacketBatch *dead_batch() const { return &dead_batch_; }
  BatchPool *batch_pool() const { return &batch_pool_; }

  static ProtoMap *protos() {
    if (!protos_)
        protos_ = std::make_shared<ProtoMap>(CONFIG.nic.socket);

    return protos_.get();
  }

private:
  // Used by Run()
  Module *module_;
  // Auxiliary value passed to Module::RunTask()
  void *arg_;
  // A packet batch for storing packets to free
  mutable PacketBatch dead_batch_;

  mutable BatchPool batch_pool_;

  static std::shared_ptr<ProtoMap> protos_;
};

} // namespace xlb

#endif // XLB_TASK_H
