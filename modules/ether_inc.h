#pragma once

#include <type_traits>

#include <glog/logging.h>

#include "modules/arp_inc.h"
#include "modules/common.h"
#include "modules/ether_out.h"
#include "modules/ipv4_inc.h"

#include "utils/lock_less_queue.h"

namespace xlb::modules {

class EtherInc final : public Module {
 public:
  EtherInc(uint8_t weight)
      : weight_(weight),
        kni_ring_(CONFIG.nic.socket, CONFIG.kni.ring_size, true, false) {
    DLOG(INFO) << "Init with weight: " << static_cast<int>(weight_)
               << " ring size: " << kni_ring_.Capacity();
  }

  void InitInSlave(uint16_t wid) override {
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch{};

          if (!kni_ring_.Empty()) {
            batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));

            DLOG(INFO) << "Pop " << batch.cnt()
                       << " packets from EtherInc's ring";

            HandOn<PortOut<PMD>>(ctx, &batch);
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
        DLOG(ERROR) << "Too many packets from kernel are on the fly";

      DLOG(INFO) << "Push " << batch->cnt() << " packets to EtherInc's ring";
    } else {
      PacketBatch ipv4_batch{};

      for (Packet &p : *batch) {
        auto &type = p.head_data<Ethernet *>()->ether_type;

        if (likely(type == be16_t(Ethernet::kIpv4)))
          ipv4_batch.Push(&p);
        else if (type == be16_t(Ethernet::kArp))
          ctx->HoldPacket(Handle<ArpInc>(ctx, &p));
        else
          ctx->DropPacket(&p);
      }

      if (batch->cnt() > 0) HandOn<Ipv4Inc>(ctx, &ipv4_batch);

      if (!ctx->stage_batch().Empty()) {
        HandOn<EtherOut, KNI>(ctx, &ctx->stage_batch());

        DLOG(INFO) << "Flush " << ctx->stage_batch().cnt() << " packets to kni";

        ctx->stage_batch().Clear();
      }
    }
  }

 private:
  uint8_t weight_;
  utils::LockLessQueue<Packet *> kni_ring_;
};

}  // namespace xlb::modules
