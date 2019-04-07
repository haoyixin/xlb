#pragma once

#include <memory>

#include <glog/logging.h>

#include "utils/allocator.h"
#include "utils/boost.h"
#include "utils/common.h"
#include "utils/endian.h"
#include "utils/format.h"
#include "utils/time.h"
#include "utils/timer.h"

#include "headers/ip.h"
#include "headers/tcp.h"

#include "config.h"
#include "worker.h"

namespace xlb {

using utils::be16_t;
using utils::be32_t;
using utils::be64_t;
using utils::irange;

using utils::EventBase;
using utils::Rdtsc;
using utils::TimerWheel;
using utils::tsc_ms;
using utils::tsc_sec;

using std::make_shared;
using std::shared_ptr;
using utils::INew;
using utils::intrusive_ref_counter;
using utils::unsafe_intrusive_ref_counter;

using headers::Tcp;
using headers::ToIpv4Address;

}  // namespace xlb
