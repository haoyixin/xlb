#ifndef XLB_MODULE_H
#define XLB_MODULE_H

#include <string>

#include "packet_batch.h"
#include "packet_pool.h"

#include "utils/cuckoo_map.h"

namespace xlb {

class Task;

struct Context {
  // Set by task scheduler, read by modules
  uint64_t current_tsc;
  uint64_t current_ns;
  int wid;
  Task *task;

  // Set by module scheduler, read by a task scheduler
  uint64_t silent_drops;
};

class Module {
  // overide this section to create a new module -----------------------------
public:
  virtual ~Module();

  // Initiates a new task with 'ctx', generating a new workload (a set of
  // packets in 'batch') and forward the workloads to be processed and forward
  // the workload to next modules. It can also get per-module specific 'arg'
  // as input. 'batch' is pre-allocated for efficiency.
  // It returns info about generated workloads, 'task_result'.
  virtual struct task_result RunTask(Context *ctx, PacketBatch *batch,
                                     void *arg);

  // Process a set of packets in packet batch with the contexts 'ctx'.
  // A module should handle all packets in a batch properly as follows:
  // 1) forwards to the next modules, or 2) free
  virtual void ProcessBatch(Context *ctx, PacketBatch *batch);

  // -------------------------------------------------------------------------
protected:
  explicit Module(std::string &name) : name_(name), registered_(false) {}

  template <typename T>
  inline void NextModule(T *mod, Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    mod->ProcessBatch(ctx, batch);
  }

  // With the contexts('ctx'), drop a packet. Dropped packets will be freed.
  inline void DropPacket(Context *ctx, Packet *pkt);

  // Register a task.
  void RegisterTask(void *arg);

  const std::string &name() const { return name_; }

private:
  std::string name_;

  bool registered_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
};

static inline void deadend(Context *ctx, PacketBatch *batch) {
  ctx->silent_drops += batch->cnt();
  Packet::Free(batch);
  batch->clear();
}

} // namespace xlb

// TODO: maybe a module builder is necessary

#define DEFINE_MODULE(_MOD) _MOD *MODULES_##_MOD

#define DECLARE_MODULE(_MOD) extern _MOD *MODULES_##_MOD

#define MODULE_INIT(_MOD, ...) MODULES_##_MOD = new _MOD(#_MOD, __VA_ARGS__)

#endif // XLB_MODULE_H
