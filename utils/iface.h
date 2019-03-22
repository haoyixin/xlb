#pragma once

#include <string>

#include "headers/ether.h"

namespace xlb::utils {

bool SetHwAddr(const std::string &ifname,
               const headers::Ethernet::Address &addr);
/*
 * TODO: fix it
bool SetIpAddr(const std::string &ifname, const std::string &addr);
bool SetNetmask(const std::string &ifname, const std::string &netmask);
bool SetUp(const std::string &ifname);
 */

}  // namespace xlb::utils
