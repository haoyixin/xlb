#pragma once

#include "modules/common.h"

namespace xlb::modules {

class TcpInc : public Module {
 public:
  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    for (Packet &p : *batch) {
      auto *ip_hdr = p.head_data<Ipv4 *>(sizeof(Ethernet));
      auto *tcp_hdr =
          p.head_data<Tcp *>(sizeof(Ethernet) + ip_hdr->header_length);

      DLOG(INFO) << "Process tcp packet from: " << ToIpv4Address(ip_hdr->src)
                 << " port: " << tcp_hdr->src_port
                 << " to: " << ToIpv4Address(ip_hdr->dst)
                 << " port: " << tcp_hdr->dst_port
                 << " in worker: " << ctx->worker()->current()->id();

      ctx->DropPacket(&p);
    }

    // TODO:: come on
  }

 private:
};

}  // namespace xlb::modules
