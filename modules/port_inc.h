#pragma once

#include "modules/common.h"
#include "modules/ether_inc.h"

namespace xlb::modules {

template <typename T, bool Master = false>
class PortInc final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

 public:
  template <typename... Args>
  explicit PortInc(uint8_t weight, Args &&... args)
      : weight_(weight),
        port_(Singleton<T>::instance(std::forward<Args>(args)...)) {}

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
          PacketBatch batch{};
          batch.SetCnt(port_.Recv(ctx->worker()->current()->id(), batch.pkts(),
                                  Packet::kMaxBurst));

          // TODO: come on
          if (batch.cnt() > 0) {
            DLOG(INFO) << "Recieve " << batch.cnt() << " packets in "
                       << (Master ? "master" : "slave") << " thread";
            HandOn<EtherInc, T>(ctx, &batch);
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
