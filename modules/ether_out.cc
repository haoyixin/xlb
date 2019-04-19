#include "modules/ether_out.h"

namespace xlb::modules {

void EtherOut::InitInMaster() {
  RegisterTask(
      [this](Context *ctx) -> Result {
        PacketBatch batch;

        batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));

        if (!batch.Empty()) Handle<PortOut<KNI>>(ctx, &batch);

        return {.packets = batch.cnt()};
      },
      kWeight);
}

template <>
void EtherOut::Process<KNI>(Context *ctx, Packet *packet) {
  while (!kni_ring_.Push(packet))
    W_LOG(ERROR) << "too many packets are on the way to kernel";
}

template <>
void EtherOut::Process<PMD>(Context *ctx, Packet *packet) {
  auto *hdr = packet->head_data<Ethernet *>();

  hdr->src_addr = CONFIG.nic.mac_address;

  //  asm volatile("" : "=m"(const_cast<Ethernet::Address &>(gw_hw_addr_)) : :);
  hdr->dst_addr = gw_hw_addr_;

  ctx->Hold(packet);
}

}  // namespace xlb::modules
