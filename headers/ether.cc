#include "ether.h"

#include <cstring>
#include <sstream>

#include "utils/copy.h"
#include "utils/endian.h"
#include "utils/format.h"
#include "utils/random.h"

namespace xlb {
namespace headers {

using Address = Ethernet::Address;

Address::Address(const std::string &str) {
  if (!FromString(str)) {
    memset(bytes, 0, sizeof(bytes));
  };
}

bool Address::FromString(const std::string &str) {
  int ret = utils::Parse(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", &bytes[0],
                  &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);
  return (ret == Address::kSize);
}

std::string Address::ToString() const {
  return utils::Format("%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", bytes[0], bytes[1],
                bytes[2], bytes[3], bytes[4], bytes[5]);
}

void Address::Randomize() {
  Random rng;

  for (size_t i = 0; i < Address::kSize; i++) {
    bytes[i] = rng.Get() & 0xff;
  }

  bytes[0] &= 0xfe;  // not broadcast/multicast
  bytes[0] |= 0x02;  // locally administered
}

}  // namespace headers
}  // namespace xlb
