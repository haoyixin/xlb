#include "modules/tcp_inc.h"

namespace xlb::modules {

template <>
void TcpInc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ip_hdr = packet->head_data<Ipv4 *>(packet->l2_len());
  auto *tcp_hdr =
      packet->head_data<Tcp *>(packet->l2_len() + packet->l3_len());

  packet->set_l4_len(tcp_hdr->offset * 4);

  if (unlikely(!packet->rx_l4_cksum_good())) {
    ctx->Drop(packet);
    W_DVLOG(1) << "Invalid tcp checksum";
  }

  // TODO: dnat -> snat -> csum

  W_DLOG(INFO) << "Tcp packet from: " << ToIpv4Address(ip_hdr->src)
             << " port: " << tcp_hdr->src_port
             << " to: " << ToIpv4Address(ip_hdr->dst)
             << " port: " << tcp_hdr->dst_port;

  W_DLOG(INFO) << packet->Dump();

  ctx->Drop(packet);
}

}  // namespace xlb::modules
