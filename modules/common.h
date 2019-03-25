#pragma once

#include <type_traits>

#include "module.h"
#include "config.h"
#include "port.h"

#include "utils/endian.h"
#include "utils/common.h"

#include "ports/kni.h"
#include "ports/pmd.h"

#include "headers/arp.h"
#include "headers/ether.h"
#include "headers/ip.h"
#include "headers/tcp.h"

namespace xlb::modules {

using std::is_same;

using utils::be16_t;
using utils::be32_t;
using utils::be64_t;

using utils::Singleton;

using ports::KNI;
using ports::PMD;

using headers::Arp;
using headers::Ethernet;
using headers::Ipv4;
using headers::Tcp;

using headers::ToIpv4Address;
using headers::ParseIpv4Address;

}  // namespace xlb::headers
