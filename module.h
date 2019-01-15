#ifndef XLB_MODULE_H
#define XLB_MODULE_H

#include <functional>
#include <string>

#include "utils/cuckoo_map.h"
#include "utils/singleton.h"

#include "packet_batch.h"
#include "packet_pool.h"

#include "task.h"
#include "types.h"

namespace xlb {

class Module {
public:
  virtual ~Module() = default;

  // Process a set of packets in packet batch with the contexts 'ctx'.
  // A module should handle all packets in a batch properly as follows:
  // 1) forwards to the next modules, or 2) free
  virtual void ProcessBatch(Context *ctx, PacketBatch *batch) { CHECK(0); }

protected:
  Module() {
    utils::Singleton<InitWorkerFuncs>::instance().emplace_back(
        [this](uint16_t wid) { this->InitWorker(wid); });
  }

  template <typename T> void NextModule(Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    utils::UnsafeSingleton<T>::instance()->ProcessBatch(ctx, batch);
  }

  // This will be called in worker thread once
  virtual void InitWorker(uint16_t wid) { CHECK(1); }

  // Register a new task by a ModuleTaskFunc, the latter should generate a set
  // of packets in 'batch' and forward them to next modules by NextModule. It
  // can also get per-task specific 'arg' as input. 'batch' is pre-allocated for
  // efficiency. It returns info about generated workloads, 'TaskResult'.
  template <typename T>
  void RegisterTask(ModuleTaskFunc<T> module_func, void *arg) {
    static_assert(std::is_base_of<Module, T>::value);
    // TODO: do not use bind
    utils::Singleton<TaskFuncs>::instance().emplace_back(
        std::bind(module_func, static_cast<T *>(this), std::placeholders::_1,
                  std::placeholders::_2, arg));
  }

  // With the contexts('ctx'), drop a packet. Dropped packets will be freed.
  void DropPacket(Context *ctx, Packet *pkt) {
    ctx->task->DropPacket(pkt);
    ctx->silent_drops += 1;
  }

  auto AllocBatch(Context *ctx) { return ctx->task->AllocBatch(); }

private:
  DISALLOW_COPY_AND_ASSIGN(Module);
};

} // namespace xlb

#endif // XLB_MODULE_H
