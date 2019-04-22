#pragma once

#include "runtime/common.h"

namespace xlb {

class Exec {
 private:
  struct Master {};
  struct Slave {};
  struct Any {};

  using Functor = std::function<void()>;
  using Channel = utils::Channel<Functor>;

 public:
  void RegisterMaster() {
    Channel::Register<Master>();
    Channel ::Register<Any>();
  }
  void RegisterSlave() {
    Channel::Register<Slave>();
    Channel ::Register<Any>();
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

  static void Sync() {
    Channel::Ptr ptr;
    // TODO: rollback when failed
    while ((ptr = Channel::Recv()) != nullptr) (*ptr)();
  }
};

}  // namespace xlb
