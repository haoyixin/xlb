#include "modules/arp_inc.h"

namespace xlb::modules {

template <>
void ArpInc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *arp_hdr = packet->head_data<Arp *>(sizeof(Ethernet));

  if (arp_hdr->opcode == be16_t(Arp::kReply) &&
      arp_hdr->sender_ip_addr == gw_ip_addr_ &&
      arp_hdr->sender_hw_addr != gw_hw_addr_) {
    W_LOG(INFO) << "alter gateway address from: " << gw_hw_addr_
                << " to: " << arp_hdr->sender_hw_addr;

    gw_hw_addr_ = arp_hdr->sender_hw_addr;
    //    asm volatile("" : : "m"(gw_hw_addr_) :);
  }

  Handle<EtherOut, KNI>(ctx, packet);
}

}  // namespace xlb::modules
