#pragma once

#include <string>

#include "headers/ether.h"

namespace xlb::utils {

bool SetHwAddr(const std::string &ifname, const headers::Ethernet::Address &addr);
bool SetIpAddr(const std::string &ifname, const headers::be32_t &addr);

}  // namespace xlb::utils
