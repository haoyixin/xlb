#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

#include <glog/logging.h>

#include "utils/copy.h"
#include "utils/endian.h"
#include "utils/format.h"
#include "utils/random.h"

namespace xlb::headers {

using utils::be16_t;
using utils::be32_t;
using utils::be64_t;

using utils::CopyInlined;
using utils::Format;
using utils::Parse;
using utils::Random;

}  // namespace xlb::headers
