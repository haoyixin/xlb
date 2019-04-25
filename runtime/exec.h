#pragma once

#include "runtime/common.h"
#include "runtime/worker.h"

namespace xlb {

class Exec {
 private:
  struct Master {};
  struct Slave {};
  struct Any {};
  struct Trivial {};

  using Functor = std::function<void()>;
  using Channel = utils::Channel<Functor>;

 public:
  static void RegisterMaster() {
    Channel::Register<Master>(CONFIG.execute_channel_size);
    Channel::Register<Any>(CONFIG.execute_channel_size);
  }
  static void RegisterSlave() {
    Channel::Register<Slave>(CONFIG.execute_channel_size);
    Channel::Register<Any>(CONFIG.execute_channel_size);
  }
  static void RegisterTrivial() {
    Channel::Register<Trivial>(CONFIG.execute_channel_size);
    Channel::Register<Any>(CONFIG.execute_channel_size);
  }

  template <typename... Args>
  static void InMaster(Args &&... args) {
    Channel::Send<Master>(std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void InSlaves(Args &&... args) {
    Channel::Send<Slave>(std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void InAny(Args &&... args) {
    Channel::Send<Slave>(std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void InTrivial(Args &&... args) {
    Channel::Send<Trivial>(std::forward<Args>(args)...);
  }

  static size_t Sync() {
    Channel::Ptr ptr;
    size_t cnt = 0;

    // TODO: rollback when failed
    while ((ptr = Channel::Recv()) != nullptr) {
      ++cnt;
      (*ptr)();
    }

    return cnt;
  }
};

}  // namespace xlb
