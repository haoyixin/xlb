#pragma once

#include <algorithm>
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

  virtual void Process(Context *ctx, PacketBatch *batch) { CHECK(0); }
  virtual void Process(Context *ctx, Packet *packet) { CHECK(0); }

  virtual void InitInWorker(uint16_t wid) { CHECK(1); }

 protected:
  Module() { Modules::instance().emplace_back(this); }

  void RegisterTask(Func &&func, uint8_t weight) {
    auto worker = Worker::current();

    // This must be called in worker thread
    CHECK(!Worker::current());
    worker->scheduler()->RegisterTask(std::move(func), weight);
  }

  template <typename T>
  void Next(Context *ctx, PacketBatch *batch) {
    static_assert(std::is_base_of<Module, T>::value);
    utils::UnsafeSingleton<T>::instance()->Process(ctx, batch);
  }

  template <typename T>
  void Next(Context *ctx, Packet *packet) {
    static_assert(std::is_base_of<Module, T>::value);
    utils::UnsafeSingleton<T>::instance()->Process(ctx, packet);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Module);
};

}  // namespace xlb
