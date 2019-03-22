#pragma once

#include "modules/common.h"

#include "modules/port_out.h"
#include "utils/lock_less_queue.h"

namespace xlb::modules {

class EtherOut : public Module {
 public:
  EtherOut(uint8_t weight)
      : weight_(weight),
        gw_addr_(Singleton<Ethernet::Address>::instance()),
        kni_ring_(CONFIG.nic.socket, CONFIG.kni.ring_size, false, true) {}

  void InitInMaster() override {
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch{};

          if (!kni_ring_.Empty()) {
            batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));

            DLOG(INFO) << "Pop " << batch.cnt()
                       << " packets from EtherOut's ring";

            HandOn<PortOut<KNI>>(ctx, &batch);
          }

          return {.packets = batch.cnt()};
        },
        weight_);
  }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    if constexpr (std::is_same<Tag, KNI>::value) {
      using Error = decltype(kni_ring_)::Error;

      DLOG(INFO) << "Ring size: " << kni_ring_.Size();

      while (kni_ring_.Push(batch->pkts(), batch->cnt()) == Error::NOBUF)
        LOG_EVERY_N(ERROR, 10) << "Too many packets are on the way to kernel";

      DLOG(INFO) << "Push " << batch->cnt() << " packets to EtherOut's ring";
    } else {
      asm volatile("" : "=m"(gw_addr_) : :);
      for (auto &p : *batch) p.head_data<Ethernet *>()->dst_addr = gw_addr_;

      HandOn<PortOut<PMD>>(ctx, batch);
    }
  }

 private:
  uint8_t weight_;
  Ethernet::Address &gw_addr_;
  utils::LockLessQueue<Packet *> kni_ring_;
};

}  // namespace xlb::modules
