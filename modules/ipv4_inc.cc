#include "modules/ipv4_inc.h"
#include "modules/ether_out.h"

namespace xlb::modules {

//bool Ipv4Inc::fragmented(const Ipv4 *hdr) const {
//  return (hdr->fragment_offset & be16_t(hdr->kMF)) != be16_t(0) ||
//         (hdr->fragment_offset & be16_t(hdr->kMF - 1)) != be16_t(0);
//}

template <>
void Ipv4Inc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ipv4_hdr = packet->head_data<Ipv4 *>(packet->l2_len());
  packet->set_l3_len(ipv4_hdr->header_length * 4);

  if (unlikely(!packet->rx_ip_cksum_good())) {
    ctx->Drop(packet);
    W_DVLOG(1) << "Invalid ipv4 checksum";
  }

  if (unlikely(ipv4_hdr->mf ||
               ipv4_hdr->fragment_offset & be16_t(Ipv4::kOffsetMask))) {
    ctx->Drop(packet);
    W_DVLOG(1) << "Fragmented ipv4 packet";
  }

  if (unlikely(ipv4_hdr->dst == kni_ip_addr_))
    Handle<EtherOut, KNI>(ctx, packet);
  else if (likely(ipv4_hdr->protocol == Ipv4::kTcp))
    Handle<TcpInc, PMD>(ctx, packet);
  else
    ctx->Drop(packet);
}

}  // namespace xlb::modules
