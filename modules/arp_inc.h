#pragma once

#include "config.h"

#include "modules/common.h"

namespace xlb::modules {

class ArpInc : public Module {
 public:
  ArpInc()
      : gw_ip_addr_(Singleton<be32_t, ArpInc>::instance()),
        gw_hw_addr_(Singleton<Ethernet::Address>::instance()) {
    ParseIpv4Address(CONFIG.kni.gateway, &gw_ip_addr_);
  }

  template <typename Tag = NoneTag>
  Packet *Process(Context *ctx, Packet *packet) {
    auto *arp_hdr = packet->head_data<Arp *>(sizeof(Ethernet));

    if (arp_hdr->opcode == be16_t(Arp::kReply) &&
        arp_hdr->sender_ip_addr == gw_ip_addr_ &&
        arp_hdr->sender_hw_addr != gw_hw_addr_) {
      DLOG(INFO) << "Alter gateway address from " << gw_hw_addr_.ToString()
                 << " to " << arp_hdr->sender_hw_addr.ToString();

      gw_hw_addr_ = arp_hdr->sender_hw_addr;
      asm volatile("" : : "m"(gw_hw_addr_) :);
    }

    return packet;
  }

 private:
  be32_t &gw_ip_addr_;
  Ethernet::Address &gw_hw_addr_;
};

}  // namespace xlb::modules
