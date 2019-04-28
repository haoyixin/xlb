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
  virtual void InitInTrivial() { CHECK(1); };

  template <Worker::Type type>
  void InitIn(uint16_t wid) {
    if constexpr (type == Worker::Slave)
      InitInSlave(wid);
    else if constexpr (type == Worker::Master)
      InitInMaster();
    else if constexpr (type == Worker::Trivial)
      InitInTrivial();
    else
      CHECK(0);
  }

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

  // T must be a type_string, such as 'TS("your_task")'
  template <typename T>
  void RegisterTask(Func &&func) {
    auto worker = Worker::current();
    DCHECK_NOTNULL(worker);

    W_LOG(INFO) << "module: " << module_name() << " name: " << T::data();
    worker->scheduler()->RegisterTask(std::move(func), T::data());
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

// TODO: solve the problem of include recursion
// The entry point of worker threads
template <Worker::Type T>
inline void *Worker::run() {
  random_ = new utils::Random();

  std::string name;

  name = utils::Format("%s@%u", type_str<T>(), core_);

  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_), &cpu_set_);
  pthread_setname_np(pthread_self(), name.c_str());

  // DPDK lcore ID == worker ID (0, 1, 2, 3, ...)
  RTE_PER_LCORE(_lcore_id) = id_;

  // shouldn't be SOCKET_ID_ANY (-1)
  CHECK_GE(socket_, 0);
  CHECK_NOTNULL(packet_pool_);

  for (;;) {
    if (starting<T>()) break;
  }

  W_LOG(INFO) << "starting on core: " << core_ << " socket: " << socket_;

  scheduler_ = new Scheduler();
  scheduler_->Loop<T>();

  W_LOG(INFO) << "quitting on core: " << core_ << " socket: " << socket_;
  MarkAborted<T>();

  delete scheduler_;
  delete random_;

  return nullptr;
}

template <Worker::Type T>
inline void Scheduler::Loop() {
  bool idle{true};
  uint64_t packets{0};
  Task::Context *ctx;
  uint64_t cycles{};

  W_CURRENT->UpdateTsc();

  for (auto &m : Modules::instance()) m->InitIn<T>(W_ID);

  Worker::MarkStarted<T>();

  auto execute = [this, &ctx, &idle]() {
    ctx = next_ctx();
    ctx->silent_drops_ = 0;
    idle = ctx->task_->execute(ctx);
  };

  W_CURRENT->UpdateTsc();
  checkpoint_ = W_TSC;
  execute();

  // The main scheduling, running, accounting master loop.
  for (;;) {
    if (Worker::aborting<T>()) break;

    W_CURRENT->UpdateTsc();
    cycles = W_TSC - checkpoint_;
    ctx->task_->update_weight(idle, cycles);
    update_cpu_usage<T>(idle, cycles);
    checkpoint_ = W_TSC;

    execute();
  }
}

}  // namespace xlb
