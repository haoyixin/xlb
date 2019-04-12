#pragma once

#include "modules/common.h"

namespace xlb::modules {

template <typename T>
class PortOut final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  explicit PortOut() : port_(Singleton<T>::instance()) {}

  //  ~PortOut() { delete (port_); }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    port_.Send(W_ID, batch->pkts(), batch->cnt());
  }

  //  template <typename Tag = NoneTag>
  //  void Process(Context *ctx, Packet *packet) override {
  //    port_->Send(ctx->worker()->current()->id(), &packet, 1);
  //  }

 private:
  T &port_;
};

}  // namespace xlb::modules
