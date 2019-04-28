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
  inline void Process(Context *ctx, Packet *packet);

 private:
  be32_t &kni_ip_addr_;
};

}  // namespace xlb::modules
