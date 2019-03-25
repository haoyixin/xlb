#include "modules/ether_inc.h"

namespace xlb::modules {

void EtherInc::InitInSlave(uint16_t wid) {
  RegisterTask(
      [this](Context *ctx) -> Result {
        PacketBatch batch{};

        if (!kni_ring_.Empty()) {
          batch.SetCnt(kni_ring_.Pop(batch.pkts(), Packet::kMaxBurst));
          Handle<PortOut<PMD>>(ctx, &batch);
        }

        return {.packets = batch.cnt()};
      },
      weight_);
}

template <>
void EtherInc::Process<KNI>(Context *ctx, PacketBatch *batch) {
  while (!kni_ring_.Push(batch->pkts(), batch->cnt()))
    LOG(ERROR) << "Too many packets from kernel are on the fly";
}

template <>
void EtherInc::Process<PMD>(Context *ctx, PacketBatch *batch) {

  for (Packet &p : *batch) {
    auto &type = p.head_data<Ethernet *>()->ether_type;

    if (likely(type == be16_t(Ethernet::kIpv4)))
      Handle<Ipv4Inc, PMD>(ctx, &p);
    else if (type == be16_t(Ethernet::kArp))
      Handle<ArpInc, PMD>(ctx, &p);
    else
      ctx->Drop(&p);
  }

  if (!ctx->stage_batch().Empty()) {
    Handle<PortOut<PMD>>(ctx, &ctx->stage_batch());
    ctx->stage_batch().Clear();
  }
}

}  // namespace xlb::modules
