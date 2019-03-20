#pragma once

#include "config.h"

#include "modules/common.h"

namespace xlb::modules {

class ArpInc : public Module {
 public:
  template <typename Tag = NoneTag>
  Packet *Process(Context *ctx, Packet *packet) {
    auto arp_hdr = packet->head_data<Arp *>(sizeof(Ethernet));

    if (arp_hdr->opcode = be16_t(Arp::kReply)) {
      gw_addr_ = packet->head_data<Arp *>()->sender_hw_addr;
      asm volatile("" : : "m"(gw_addr_) :);
    }

    return packet;
  }

 private:
  Ethernet::Address &gw_addr_;
};

}  // namespace xlb::modules
