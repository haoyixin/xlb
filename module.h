#ifndef XLB_MODULE_H
#define XLB_MODULE_H

#include "context.h"
#include "task.h"

#include "packet_batch.h"
#include "packet_pool.h"

#include "utils/cuckoo_map.h"

#include <functional>
#include <string>
#include <utils/singleton.h>

#include "types.h"

namespace xlb {

class Module {
public:
  virtual ~Module() = default;

  // Process a set of packets in packet batch with the contexts 'ctx'.
  // A module should handle all packets in a batch properly as follows:
  // 1) forwards to the next modules, or 2) free
  virtual void ProcessBatch(Context *ctx, PacketBatch *batch) = 0;

protected:
  explicit Module(std::string &name) : name_(name) {}

  template <typename T>
  void NextModule(T *mod, Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    mod->ProcessBatch(ctx, batch);
  }

  // Register a new task by a ModuleFunc, the latter should generate a set of
  // packets in 'batch' and forward them to next modules by NextModule. It can
  // also get per-task specific 'arg' as input. 'batch' is pre-allocated for
  // efficiency. It returns info about generated workloads, 'TaskResult'.
  template <typename T>
  void RegisterTask(ModuleFunc<T> module_func, void *arg) {
    static_assert(std::is_base_of<Module, T>::value);
    utils::Singleton<TaskFuncs>::Get().emplace_back(
        std::bind(module_func, static_cast<T *>(this), std::placeholders::_1,
                  std::placeholders::_2, arg));
  }

  // With the contexts('ctx'), drop a packet. Dropped packets will be freed.
  void DropPacket(Context *ctx, Packet *pkt) {
    ctx->task->DropPacket(pkt);
    ctx->silent_drops += 1;
  }

  auto AllocBatch(Context *ctx) { return ctx->task->AllocBatch(); }

  const std::string &name() const { return name_; }

private:
  std::string name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
};

} // namespace xlb

// TODO: maybe a module builder is necessary

#define DEFINE_MODULE(_MOD) xlb::modules::_MOD *MODULES_##_MOD

#define DECLARE_MODULE(_MOD) extern xlb::modules::_MOD *MODULES_##_MOD

#define MODULE_INIT(_MOD, ...)                                                 \
  MODULES_##_MOD = new xlb::modules::_MOD(#_MOD, ##__VA_ARGS__)

#endif // XLB_MODULE_H
