#pragma once

#include "runtime/common.h"

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
    Channel::Register<Master>(CONFIG.rpc.max_concurrency);
    Channel::Register<Any>(CONFIG.rpc.max_concurrency);
  }
  static void RegisterSlave() {
    Channel::Register<Slave>(CONFIG.rpc.max_concurrency);
    Channel::Register<Any>(CONFIG.rpc.max_concurrency);
  }
  static void RegisterTrivial() {
    Channel::Register<Trivial>(CONFIG.rpc.max_concurrency);
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
