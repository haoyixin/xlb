#pragma once

#include "modules/common.h"

namespace xlb::modules {

template <typename T>
class PortOut final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  template <typename... Args>
  explicit PortOut(Args &&... args)
      : port_(Singleton<T>::instance(std::forward<Args>(args)...)) {}

  //  ~PortOut() { delete (port_); }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    port_.Send(ctx->worker()->current()->id(), batch->pkts(), batch->cnt());
    DLOG(INFO) << "Send " << batch->cnt()
               << " packets in worker: " << ctx->worker()->current()->id();
  }

  //  template <typename Tag = NoneTag>
  //  void Process(Context *ctx, Packet *packet) override {
  //    port_->Send(ctx->worker()->current()->id(), &packet, 1);
  //  }

 private:
  T &port_;
};

}  // namespace xlb::modules
