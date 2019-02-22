#pragma once

#include "config.h"
#include "module.h"

#include "headers/ether.h"

namespace xlb::modules {

template <typename T>
class PortOut final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  template <typename... Args>
  explicit PortOut(Args &&... args) {
    port_ = &utils::Singleton<T>::instance(std::forward<Args>(args)...);
  }

  ~PortOut() {
    delete(port_);
  }

  inline void Process(Context *ctx, PacketBatch *batch) override {
    port_->Send(ctx->worker()->current()->id(), batch->pkts(), batch->cnt());
  }

  inline void Process(Context *ctx, Packet *packet) override {
    port_->Send(ctx->worker()->current()->id(), &packet, 1);
  }

 private:
  T *port_;
};

}  // namespace xlb::modules
