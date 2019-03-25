#pragma once

#include "modules/common.h"

#include "modules/port_out.h"
#include "utils/lock_less_queue.h"

namespace xlb::modules {

class EtherOut : public Module {
 public:
  EtherOut(uint8_t weight)
      : weight_(weight),
        src_hw_addr_(CONFIG.nic.mac_address),
        gw_hw_addr_(Singleton<Ethernet::Address>::instance()),
        kni_ring_(CONFIG.nic.socket, CONFIG.kni.ring_size) {
    DLOG(INFO) << "Source mac address: " << src_hw_addr_.ToString();
  }

  void InitInMaster() override;

  template <typename Tag = NoneTag>
  void Process(Context *ctx, Packet *packet);

  //  template <typename Tag = NoneTag>
  //  void Process(Context *ctx, PacketBatch *batch);

 private:
  uint8_t weight_;
  const Ethernet::Address &src_hw_addr_;
  const Ethernet::Address &gw_hw_addr_;
  utils::LockLessQueue<Packet *, false, true> kni_ring_;
};

}  // namespace xlb::modules
