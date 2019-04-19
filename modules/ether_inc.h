#pragma once

#include "modules/arp_inc.h"
#include "modules/common.h"
#include "modules/ether_out.h"
#include "modules/ipv4_inc.h"

namespace xlb::modules {

class EtherInc final : public Module {
 public:
  EtherInc() : kni_ring_(CONFIG.kni.ring_size) {
    F_DLOG(INFO) << "init with weight: "
                 << " ring size: " << kni_ring_.Capacity();
  }

  void InitInSlave(uint16_t wid) override;

  template <typename Tag = NoneTag>
  inline void Process(Context *ctx, PacketBatch *batch);

 private:
  // For sending packets received from kni to pmd (in slave)
  static constexpr uint8_t kWeight = 10;
  utils::LockLessQueue<Packet *, true, false> kni_ring_;
};

}  // namespace xlb::modules
