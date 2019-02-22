#pragma once

#include "module.h"
#include "port.h"
#include "ports/pmd.h"
#include "worker.h"

namespace xlb::modules {

template <typename T>
class PortInc final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  template <typename... Args>
  explicit PortInc(Args &&... args) {
    port_ = &utils::Singleton<T>::instance(std::forward<Args>(args)...);
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch{};
          batch.SetCnt(port_->Recv(ctx->worker()->current()->id(), batch.pkts(),
                                   Packet::kMaxBurst));

          // TODO: come on

          return {.packets = batch.cnt()};
        },
        kWeight);
  }

  ~PortInc() { delete (port_); }

 private:
  static const uint8_t kWeight = 200;
  T *port_;
};

}  // namespace xlb::modules
