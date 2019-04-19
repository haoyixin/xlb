#pragma once

#include "modules/common.h"
#include "modules/ether_out.h"

namespace xlb::modules {

class CSum : Module {
 public:
  template <typename Tag = NoneTag>
  void Process(Context *ctx, Packet *packet) {
    static_assert(is_same<Tag, Ipv4>::value || is_same<Tag, Tcp>::value);

    auto *ipv4_hdr = packet->head_data<Ipv4 *>(sizeof(Ethernet));
    auto ipv4_hdr_len = ipv4_hdr->header_length * 4;

    packet->set_l2_len(sizeof(Ethernet));
    packet->set_l3_len(ipv4_hdr_len);

    ipv4_hdr->checksum = 0;
    packet->set_ol_flags(PKT_TX_IPV4 | PKT_TX_IP_CKSUM);

    if constexpr (std::is_same<Tag, Tcp>::value) {
      auto *tcp_hdr = packet->head_data<Tcp *>(sizeof(Ethernet) + ipv4_hdr_len);

      packet->set_l4_len(tcp_hdr->offset * 4);
      tcp_hdr->checksum = 0;
      packet->set_ol_flags(PKT_TX_TCP_CKSUM);
    }

    Handle<EtherOut, PMD>(ctx, packet);
  }
};

}  // namespace xlb::modules
