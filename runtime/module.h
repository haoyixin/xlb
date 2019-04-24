#pragma once

#include "runtime/common.h"
#include "runtime/packet_batch.h"
#include "runtime/packet_pool.h"
#include "runtime/scheduler.h"
#include "runtime/worker.h"

namespace xlb {

using Modules = utils::Singleton<utils::vector<class Module *>>;

class Module {
 protected:
  using Task = Scheduler::Task;
  using Context = Task::Context;
  using Result = Task::Result;
  using Func = Task::Func;

 public:
  virtual ~Module() = default;

  // The following methods can be implemented on a subclass as needed.
  //
  // template <typename Tag = NoneTag>
  // void Process(Context *ctx, PacketBatch *batch);
  //
  // template <typename Tag = NoneTag>
  // Packet *Process(Context *ctx, Packet *packet);

  /***************************************************************************
   'Tag' represents the source 'Port' type of the 'Packet' or 'PacketBatch' in
   the 'Module' suffixed with 'Inc', and vice versa, in the 'Module' with the
   suffix of 'Out', it represents the destination. We use these rules to make
   it easier for the compiler to check for errors.
   ***************************************************************************/

  virtual void InitInSlave(uint16_t wid) { CHECK(1); }
  virtual void InitInMaster() { CHECK(1); };

  template <typename T, typename... Args>
  static void Init(Args &&... args) {
    utils::UnsafeSingleton<T>::Init(std::forward<Args>(args)...);
  }

 protected:
  // Since the error hints of modern compilers are friendly enough, we may
  // not need this.
  //  template <typename T, typename I, typename Tag>
  //  using has_processor = decltype(std::declval<T &>().template Process<Tag>(
  //      std::declval<Context *>(), std::declval<I *>()));

  struct NoneTag {};

  Module() { Modules::instance().emplace_back(this); }

  void RegisterTask(Func &&func, uint32_t weight) {
    auto worker = Worker::current();
    DCHECK_NOTNULL(worker);

    W_LOG(INFO) << "module: " << module_name()
                << " weight: " << weight;
    worker->scheduler()->RegisterTask(std::move(func), weight);
  }

  // Avoid virtual function calls, reduce branch prediction failure, and let the
  // compiler do deeper optimization
  template <typename T, typename Tag = NoneTag>
  void Handle(Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    //    static_assert(std::experimental::is_detected_exact_v<void,
    //    has_processor, T, PacketBatch, Tag>);
    utils::UnsafeSingleton<T>::instance().template Process<Tag>(ctx, batch);
  }

  template <typename T, typename Tag = NoneTag>
  void Handle(Context *ctx, Packet *packet) {
    static_assert(std::is_base_of<Module, T>::value);
    //    static_assert(std::experimental::is_detected_exact_v<void,
    //    has_processor, T, Packet, Tag>);
    return utils::UnsafeSingleton<T>::instance().template Process<Tag>(ctx,
                                                                       packet);
  }

 private:
  std::string module_name() { return demangle(this).substr(14); }

  DISALLOW_COPY_AND_ASSIGN(Module);
};

// TODO: ......

template <bool master>
void Scheduler::Loop() {
  bool idle{true};
  Task::Context *ctx;
  uint64_t cycles{};

  W_CURRENT->UpdateTsc();

  if constexpr (master)
    for (auto &m : Modules::instance()) m->InitInMaster();
  else
    for (auto &m : Modules::instance()) m->InitInSlave(W_ID);

  if constexpr (master) Worker::confirm_master();

  W_CURRENT->UpdateTsc();
  checkpoint_ = W_TSC;

  // The main scheduling, running, accounting master loop.
  for (uint64_t round = 0;; ++round) {
    if constexpr (master) {
      if (Worker::slaves_aborted()) break;
    } else {
      if (Worker::aborting()) break;
    }

    ctx = next_ctx();
    ctx->silent_drops_ = 0;

    W_CURRENT->UpdateTsc();
    cycles = W_TSC - checkpoint_;

    if (idle) {
      if constexpr (master)
        M::Adder<TS("idle_cycles_master")>() << cycles;
      else
        M::Adder<TS("idle_cycles_slaves")>() << cycles;
    } else {
      W_CURRENT->IncrBusyLoops();
      if constexpr (master)
        M::Adder<TS("busy_cycles_master")>() << cycles;
      else
        M::Adder<TS("busy_cycles_slaves")>() << cycles;
    }

    checkpoint_ = W_TSC;

    idle = (ctx->task_->func_(ctx).packets == 0);
  }
}

}  // namespace xlb
