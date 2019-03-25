#include "modules/tcp_inc.h"

namespace xlb::modules {

template <>
void TcpInc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ip_hdr = packet->head_data<Ipv4 *>(sizeof(Ethernet));
  auto *tcp_hdr =
      packet->head_data<Tcp *>(sizeof(Ethernet) + ip_hdr->header_length * 4);

  if (unlikely(!packet->rx_l4_cksum_good())) {
    ctx->Drop(packet);
    DLOG(ERROR) << "Invalid tcp checksum";
  }

  // TODO: dnat -> snat -> csum

  DLOG(INFO) << "Tcp packet from: " << ToIpv4Address(ip_hdr->src)
             << " port: " << tcp_hdr->src_port
             << " to: " << ToIpv4Address(ip_hdr->dst)
             << " port: " << tcp_hdr->dst_port
             << " in worker: " << ctx->worker()->current()->id();

  DLOG(INFO) << packet->Dump();

  ctx->Drop(packet);
}

}  // namespace xlb::modules
