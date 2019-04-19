#pragma once

#include "modules/common.h"
#include "modules/port_out.h"

namespace xlb::modules {

class EtherOut : public Module {
 public:
  EtherOut()
      : gw_hw_addr_(Singleton<Ethernet::Address>::instance()),
        kni_ring_(CONFIG.kni.ring_size) {
    F_LOG(INFO) << "source mac address: " << CONFIG.nic.mac_address.ToString();
  }

  void InitInMaster() override;

  template <typename Tag = NoneTag>
  inline void Process(Context *ctx, Packet *packet);

  //  template <typename Tag = NoneTag>
  //  void Process(Context *ctx, PacketBatch *batch);

 private:
  // For sending packets received from pmd to kni (in master)
  static constexpr uint8_t kWeight = 200;

  //  uint8_t weight_;
  //  const Ethernet::Address &src_hw_addr_;
  const Ethernet::Address &gw_hw_addr_;
  utils::LockLessQueue<Packet *, false, true> kni_ring_;
};

}  // namespace xlb::modules
