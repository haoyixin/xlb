#pragma once

#include "modules/common.h"
#include "modules/tcp_inc.h"

namespace xlb::modules {

class Ipv4Inc : public Module {
 public:
  Ipv4Inc() : kni_ip_addr_(Singleton<be32_t, Ipv4Inc>::instance()) {
    ParseIpv4Address(CONFIG.kni.ip_address, &kni_ip_addr_);
  }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch) {
    PacketBatch tcp_batch{};

    for (Packet &p : *batch) {
      auto *ipv4_hdr = p.head_data<Ipv4 *>(sizeof(Ethernet));

      if (unlikely(ipv4_hdr->dst == kni_ip_addr_))
        ctx->HoldPacket(&p);
      else if (likely(ipv4_hdr->protocol == Ipv4::kTcp))
        tcp_batch.Push(&p);
      else
        ctx->DropPacket(&p);
    }

    DLOG(INFO) << "Process " << batch->cnt() << " packets in Ipv4Inc";

    HandOn<TcpInc>(ctx, &tcp_batch);
  }

 private:
  be32_t &kni_ip_addr_;
};

}  // namespace xlb::modules
