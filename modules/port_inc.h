#pragma once

#include "modules/common.h"
#include "modules/ether_inc.h"

namespace xlb::modules {

template <typename T, bool Master = false>
class PortInc final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  explicit PortInc(uint8_t weight)
      : weight_(weight), port_(Singleton<T>::instance()) {}

  void InitInMaster() override {
    if constexpr (!Master) return;
    register_task();
  }

  void InitInSlave(uint16_t wid) override {
    if constexpr (Master) return;
    register_task();
  }

  //  ~PortInc() { delete (port_); }

 protected:
  void register_task() {
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch;
          batch.SetCnt(port_.Recv(W_ID, batch.pkts(), Packet::kMaxBurst));

          if (batch.cnt() > 0) {
            Handle<EtherInc, T>(ctx, &batch);
          }

          return {.packets = batch.cnt()};
        },
        weight_);
  }

 private:
  uint8_t weight_;
  T &port_;
};

}  // namespace xlb::modules
