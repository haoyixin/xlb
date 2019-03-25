#include "modules/ipv4_inc.h"
#include "modules/ether_out.h"

namespace xlb::modules {

bool Ipv4Inc::fragmented(const Ipv4 *hdr) const {
  return (hdr->fragment_offset & be16_t(hdr->kMF)) != be16_t(0) ||
         (hdr->fragment_offset & be16_t(hdr->kMF - 1)) != be16_t(0);
}

template <>
void Ipv4Inc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ipv4_hdr = packet->head_data<Ipv4 *>(sizeof(Ethernet));

  if (unlikely(!packet->rx_ip_cksum_good())) {
    ctx->Drop(packet);
    DLOG(ERROR) << "Invalid ipv4 checksum";
  }

  if (unlikely(fragmented(ipv4_hdr))) {
    ctx->Drop(packet);
    DLOG(ERROR) << "Fragmented ipv4 packet";
  }

  if (unlikely(ipv4_hdr->dst == kni_ip_addr_))
    Handle<EtherOut, KNI>(ctx, packet);
  else if (likely(ipv4_hdr->protocol == Ipv4::kTcp))
    Handle<TcpInc, PMD>(ctx, packet);
  else
    ctx->Drop(packet);
}

}
