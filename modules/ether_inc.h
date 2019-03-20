#pragma once

#include <type_traits>

#include <glog/logging.h>

#include "modules/arp_inc.h"
#include "modules/common.h"
#include "modules/ether_out.h"

#include "utils/lock_less_queue.h"

namespace xlb::modules {

class EtherInc final : public Module {
 public:
  void InitInSlave(uint16_t wid) override {
    RegisterTask(
        [this](Context *ctx) -> Result {
          PacketBatch batch{};
          batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));
          HandOn<PortOut<PMD>>(ctx, &batch);
        },
        weight_);
  }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    if constexpr (std::is_same<Tag, ports::KNI>::value) {
      using Error = decltype(kni_ring_)::Error;

      while (kni_ring_.Push(batch->pkts(), batch->cnt()) == Error::NOBUF)
        LOG_EVERY_N(ERROR, 10) << "Too many packets from kernel are on the fly";

      return;
    }

    PacketBatch ipv4_batch{};
    constexpr be16_t kIpv4{Ethernet::kIpv4};
    constexpr be16_t kArp{Ethernet::kIpv4};

    for (Packet &p : *batch) {
      auto &type = p.head_data<Ethernet *>()->ether_type;

      if (likely(type == kIpv4)) {
        if (likely(p.head_data<Ipv4 *>(sizeof(Ethernet))->dst != kni_ip_addr_))
          ipv4_batch.Push(&p);
        else
          ctx->HoldPacket(&p);
      } else if (type == kArp) {
        ctx->HoldPacket(Handle<ArpInc>(ctx, &p));
      } else {
        ctx->DropPacket(&p);
      }

      //      switch (p.head_data<Ethernet *>()->ether_type) {
      //        case be16_t(Ethernet::kIpv4):
      //          if (likely(p.head_data<Ipv4 *>(sizeof(Ethernet))->dst !=
      //                     kni_ip_addr_))
      //            ipv4_batch.Push(&p);
      //          else
      //            ctx->HoldPacket(&p);
      //
      //          break;
      //        case be16_t(Ethernet::kArp):
      //          ctx->HoldPacket(Handle<ArpInc>(ctx, &p));
      //          break;
      //        default:
      //          ctx->DropPacket(&p);
      //          break;
      //      }
    }

    if (!ctx->stage_batch().Empty()) HandOn<EtherOut, PMD>(ctx, &ipv4_batch);
  }

 private:
  uint8_t weight_;
  headers::be32_t kni_ip_addr_;
  utils::LockLessQueue<Packet *> kni_ring_;
};

}  // namespace xlb::modules
