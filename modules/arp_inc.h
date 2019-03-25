#pragma once

#include "config.h"

#include "modules/common.h"
#include "modules/ether_out.h"

namespace xlb::modules {

class ArpInc : public Module {
 public:
  ArpInc()
      : gw_ip_addr_(Singleton<be32_t, ArpInc>::instance()),
        gw_hw_addr_(Singleton<Ethernet::Address>::instance()) {
    ParseIpv4Address(CONFIG.kni.gateway, &gw_ip_addr_);
  }

  template <typename Tag = NoneTag>
  void Process(Context *ctx, Packet *packet);

 private:
  be32_t &gw_ip_addr_;
  Ethernet::Address &gw_hw_addr_;
};

}  // namespace xlb::modules
