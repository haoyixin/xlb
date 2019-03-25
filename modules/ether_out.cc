#include "modules/ether_out.h"

namespace xlb::modules {

void EtherOut::InitInMaster() {
  RegisterTask(
      [this](Context *ctx) -> Result {
        PacketBatch batch{};

        if (!kni_ring_.Empty()) {
          batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));
          Handle<PortOut<KNI>>(ctx, &batch);
        }

        return {.packets = batch.cnt()};
      },
      weight_);
}

template <>
void EtherOut::Process<KNI>(Context *ctx, Packet *packet) {
  while (!kni_ring_.Push(packet))
    LOG(ERROR) << "Too many packets are on the way to kernel";
}

template <>
void EtherOut::Process<PMD>(Context *ctx, Packet *packet) {
  //  asm volatile("" : "=m"(const_cast<Ethernet::Address &>(gw_hw_addr_)) : :);
  auto *hdr = packet->head_data<Ethernet *>();
  hdr->dst_addr = gw_hw_addr_;
  hdr->src_addr = src_hw_addr_;

  ctx->Hold(packet);
}

}  // namespace xlb::modules
