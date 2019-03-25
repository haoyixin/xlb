#pragma once

#include <type_traits>

#include <glog/logging.h>

#include "modules/arp_inc.h"
#include "modules/common.h"
#include "modules/ether_out.h"
#include "modules/ipv4_inc.h"

#include "utils/lock_less_queue.h"

namespace xlb::modules {

class EtherInc final : public Module {
 public:
  EtherInc(uint8_t weight)
      : weight_(weight), kni_ring_(CONFIG.nic.socket, CONFIG.kni.ring_size) {
    DLOG(INFO) << "Init with weight: " << static_cast<int>(weight_)
               << " ring size: " << kni_ring_.Capacity();
  }

  void InitInSlave(uint16_t wid) override;

  template <typename Tag = NoneTag>
  void Process(Context *ctx, PacketBatch *batch);

 private:
  uint8_t weight_;
  utils::LockLessQueue<Packet *, true, false> kni_ring_;
};

}  // namespace xlb::modules
