#pragma once

#include "modules/common.h"

#include "utils/lock_less_queue.h"
#include "modules/port_out.h"

namespace xlb::modules {

class EtherOut : public Module {
 public:
  void InitInMaster() override {
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch{};
          batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));
          HandOn<PortOut<KNI, true>>(ctx, &batch);
        },
        weight_);
  }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    if constexpr (std::is_same<Tag, KNI>::value) {
      using Error = decltype(kni_ring_)::Error;

      while (kni_ring_.Push(batch->pkts(), batch->cnt()) == Error::NOBUF)
      LOG_EVERY_N(ERROR, 10) << "Too many packets are on the way to kernel";

      return;
    }

    asm volatile("" : "=m"(gw_addr_) : : );
    for (auto &p : *batch)
      p.head_data<Ethernet *>()->dst_addr = gw_addr_;

    HandOn<PortOut<PMD>>(ctx, batch);
  }

 private:
  uint8_t weight_;
  utils::LockLessQueue<Packet *> kni_ring_;
  Ethernet::Address &gw_addr_;
};

}  // namespace xlb::modules
