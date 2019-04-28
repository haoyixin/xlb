#include "modules/ipv4_inc.h"
#include "modules/ether_out.h"

namespace xlb::modules {

template <>
void Ipv4Inc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ipv4_hdr = packet->head_data<Ipv4 *>(packet->l2_len());

  if (unlikely(ipv4_hdr->dst == kni_ip_addr_)) {
    Handle<EtherOut, KNI>(ctx, packet);
    return;
  }

  if (unlikely(!packet->rx_ip_cksum_good())) {
    ctx->Drop(packet);
    W_DVLOG(1) << "invalid ipv4 checksum";
    return;
  }

  if (unlikely(ipv4_hdr->mf ||
               ipv4_hdr->fragment_offset & be16_t(Ipv4::kOffsetMask))) {
    ctx->Drop(packet);
    W_DVLOG(1) << "fragmented ipv4 packet";
    return;
  }

  if (likely(ipv4_hdr->protocol == Ipv4::kTcp)) {
    packet->set_l3_len(ipv4_hdr->header_length * 4);
    Handle<TcpInc, PMD>(ctx, packet);
  } else {
    ctx->Drop(packet);
  }
}

}  // namespace xlb::modules
