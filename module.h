#pragma once

#include <algorithm>
//#include <experimental/type_traits>
#include <functional>
#include <string>

#include "utils/cuckoo_map.h"
#include "utils/singleton.h"

#include "packet_batch.h"
#include "packet_pool.h"

#include "scheduler.h"
#include "worker.h"

namespace xlb {

using Modules = utils::Singleton<std::vector<class Module *>>;

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

  virtual void InitInSlave(uint16_t wid) { CHECK(1); }
  virtual void InitInMaster() { CHECK(1); };

  template <typename T, typename... Args>
  static void Init(Args &&... args) {
    CHECK_NOTNULL(utils::UnsafeSingleton<T>::Init(std::forward<Args>(args)...));
  }

 protected:
  // Since the error hints of modern compilers are friendly enough, we may
  // not need this.
  //  template <typename T, typename I, typename Tag>
  //  using has_processor = decltype(std::declval<T &>().template Process<Tag>(
  //      std::declval<Context *>(), std::declval<I *>()));

  struct NoneTag {};

  Module() { Modules::instance().emplace_back(this); }

  void RegisterTask(Func &&func, uint8_t weight) {
    auto worker = Worker::current();

    CHECK(!Worker::current());
    worker->scheduler()->RegisterTask(std::move(func), weight);
  }

  // Avoid virtual function calls, reduce branch prediction failure, and let the
  // compiler do deeper optimization
  // 'HandOn' means that the lifetime management of packets in batch will be
  // handled by the next module.
  template <typename T, typename Tag = NoneTag>
  void HandOn(Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    //    static_assert(std::experimental::is_detected_exact_v<void,
    //    has_processor, T, PacketBatch, Tag>);
    utils::UnsafeSingleton<T>::instance().template Process<Tag>(ctx, batch);
  }

  template <typename T, typename Tag = NoneTag>
  Packet *Handle(Context *ctx, Packet *packet) {
    static_assert(std::is_base_of<Module, T>::value);
    //    static_assert(std::experimental::is_detected_exact_v<void,
    //    has_processor, T, Packet, Tag>);
    return utils::UnsafeSingleton<T>::instance().template Process<Tag>(ctx,
                                                                       packet);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Module);
};

}  // namespace xlb
