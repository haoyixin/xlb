#pragma once

#include <cstdint>

#include <memory>
#include <stack>
#include <string>
#include <string_view>

// WARNING: Must include this before the 'bvar' header file
#include <3rdparty/bvar/combiner.h>
#include <bvar/bvar.h>
#include <glog/logging.h>

#include "utils/allocator.h"
#include "utils/boost.h"
#include "utils/common.h"
#include "utils/endian.h"
#include "utils/format.h"
#include "utils/singleton.h"
#include "utils/time.h"
#include "utils/timer.h"
#include "utils/x_map.h"

#include "headers/ether.h"
#include "headers/ip.h"
#include "headers/tcp.h"

#include "runtime/config.h"
#include "runtime/worker.h"

namespace xlb {

// endian
using utils::be16_t;
using utils::be32_t;
using utils::be64_t;

// container
using utils::unordered_map;
using utils::unordered_set;
using utils::unordered_multimap;
using utils::unordered_multiset;
using utils::UnsafeSingleton;
using utils::UnsafeSingletonTLS;
using utils::vector;
using utils::XMap;

// utility
using utils::Format;
using utils::irange;
using utils::make_vector;
using utils::remove_erase_if;
using utils::find_if;
using utils::for_each;

// time
using utils::EventBase;
using utils::TimerWheel;
using utils::tsc_hz;
using utils::tsc_ms;
using utils::tsc_sec;

// pointer
using std::shared_ptr;
using utils::make_shared;
using utils::INew;
using utils::intrusive_ptr;
using utils::intrusive_ref_counter;
using utils::unsafe_intrusive_ref_counter;

// header
using headers::Ethernet;
using headers::Ipv4;
using headers::Tcp;
using headers::ToIpv4Address;

}  // namespace xlb
