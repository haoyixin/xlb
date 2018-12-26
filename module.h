#ifndef XLB_MODULE_H
#define XLB_MODULE_H

#include "context.h"
#include "task.h"

#include "packet_batch.h"
#include "packet_pool.h"

#include "utils/cuckoo_map.h"

#include <string>

namespace xlb {

class Module {
  // overide this section to create a new module -----------------------------
public:
  virtual ~Module() {
    if (registered_)
      Task::protos()->Remove(name_);
  }

  // Initiates a new task with 'ctx', generating a new workload (a set of
  // packets in 'batch') and forward the workloads to be processed and forward
  // the workload to next modules. It can also get per-module specific 'arg'
  // as input. 'batch' is pre-allocated for efficiency.
  // It returns info about generated workloads, 'Task::Result'.
  virtual Task::Result RunTask(Context *ctx, PacketBatch *batch, void *arg) {
    CHECK(0);
  }

  // Process a set of packets in packet batch with the contexts 'ctx'.
  // A module should handle all packets in a batch properly as follows:
  // 1) forwards to the next modules, or 2) free
  virtual void ProcessBatch(Context *ctx, PacketBatch *batch) { CHECK(0); }

  // -------------------------------------------------------------------------
protected:
  explicit Module(std::string &name) : name_(name), registered_(false) {}

  template <typename T>
  void NextModule(T *mod, Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    mod->ProcessBatch(ctx, batch);
  }

  // With the contexts('ctx'), drop a packet. Dropped packets will be freed.
  inline void DropPacket(Context *ctx, Packet *pkt) {
    ctx->task()->DropPacket(pkt);
    ctx->incr_silent_drops(1);
  }

  // Register a task.
  inline void RegisterTask(void *arg) {
    if (!registered_) {
      Task::protos()->Emplace(name_, this, arg);
      registered_ = true;
    }
  }

  const std::string &name() const { return name_; }

private:
  std::string name_;

  bool registered_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
};

// Decyclization of include
inline Task::Result Task::Run(Context *ctx) const {
  // Start from the module (task module)
  return module()->RunTask(ctx, AllocBatch().raw(), arg_);
}

} // namespace xlb

// TODO: maybe a module builder is necessary

#define DEFINE_MODULE(_MOD) xlb::modules::_MOD *MODULES_##_MOD

#define DECLARE_MODULE(_MOD) extern xlb::modules::_MOD *MODULES_##_MOD

#define MODULE_INIT(_MOD, ...)                                                 \
  MODULES_##_MOD = new xlb::modules::_MOD(#_MOD, ##__VA_ARGS__)

#endif // XLB_MODULE_H
