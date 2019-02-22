#pragma once

#include "headers/ip.h"

#include "utils/cuckoo_map.h"

#include "config.h"

namespace xlb::modules {

class ArpResponder : public Module {
public:
  ArpResponder() : local_ips_(CONFIG.nic.socket) {
    utils::be32_t addr;
    for (auto &ip_str : CONFIG.nic.local_ips) {
      headers::ParseIpv4Address(ip_str, &addr);
      local_ips_.Emplace(addr, true);
    }
  }

private:
  utils::CuckooMap<headers::be32_t, bool> local_ips_;
};

} // namespace xlb
