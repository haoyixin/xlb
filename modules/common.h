#pragma once

#include <type_traits>

#include <glog/logging.h>

#include <rte_mbuf.h>

#include "utils/common.h"
#include "utils/endian.h"
#include "utils/lock_less_queue.h"

#include "headers/arp.h"
#include "headers/ether.h"
#include "headers/ip.h"
#include "headers/tcp.h"

#include "ports/kni.h"
#include "ports/pmd.h"

#include "runtime/config.h"
#include "runtime/module.h"
#include "runtime/port.h"
#include "runtime/worker.h"
#include "runtime/exec.h"

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

using headers::ParseIpv4Address;
using headers::ToIpv4Address;

}  // namespace xlb::modules
