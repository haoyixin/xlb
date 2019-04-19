#pragma once

#include "utils/common.h"

namespace xlb::utils {

bool SetHwAddr(const std::string &ifname, const uint8_t *addr);
/*
 * TODO: fix it
bool SetIpAddr(const std::string &ifname, const std::string &addr);
bool SetNetmask(const std::string &ifname, const std::string &netmask);
bool SetUp(const std::string &ifname);
 */

}  // namespace xlb::utils
