#ifndef XLB_HEADERS_ETHER_H
#define XLB_HEADERS_ETHER_H

#include <cstdint>
#include <string>
#include <type_traits>

#include "utils/endian.h"

#include "utils/copy.h"

#include "headers/common.h"

namespace xlb {
namespace headers {

struct [[gnu::packed]] Ethernet {
  struct [[gnu::packed]] Address {
    Address() = default;
    Address(const uint8_t *addr) { utils::Copy(bytes, addr, kSize); }
    Address(const std::string &str);

    static const size_t kSize = 6;

    // Parses str in "aA:Bb:00:11:22:33" format and saves the address in parsed
    // Returns false if the format is incorrect.
    //   (in that case, the content of parsed is undefined.)
    bool FromString(const std::string &str);

    // Returns "aa:bb:00:11:22:33" (all in lower case)
    std::string ToString() const;

    void Randomize();

    bool IsBroadcast() const {
      return bytes[0] == 0xff && bytes[1] == 0xff && bytes[2] == 0xff &&
             bytes[3] == 0xff && bytes[4] == 0xff && bytes[5] == 0xff;
    }

    bool IsZero() const {
      return bytes[0] == 0x00 && bytes[1] == 0x00 && bytes[2] == 0x00 &&
             bytes[3] == 0x00 && bytes[4] == 0x00 && bytes[5] == 0x00;
    }

    bool operator<(const Address &o) const {
      for (size_t i = 0; i < kSize; i++) {
        if (bytes[i] < o.bytes[i]) {
          return true;
        }
      }
      return false;
    }

    bool operator==(const Address &o) const {
      for (size_t i = 0; i < kSize; i++) {
        if (bytes[i] != o.bytes[i]) {
          return false;
        }
      }
      return true;
    }

    bool operator!=(const Address &o) const {
      for (size_t i = 0; i < kSize; i++) {
        if (bytes[i] != o.bytes[i]) {
          return true;
        }
      }
      return false;
    }

    uint8_t bytes[kSize];
  };

  enum Type : uint16_t {
    kIpv4 = 0x0800,
    kArp = 0x0806,
    kVlan = 0x8100,
    kQinQ = 0x88a8,  // 802.1ad double-tagged VLAN packets
    kIpv6 = 0x86DD,
    kMpls = 0x8847,
  };

  Address dst_addr;
  Address src_addr;
  be16_t ether_type;
};

struct [[gnu::packed]] Vlan {
  be16_t tci;
  be16_t ether_type;
};

static_assert(std::is_pod<Ethernet>::value, "not a POD type");
static_assert(std::is_pod<Ethernet::Address>::value, "not a POD type");
static_assert(sizeof(Ethernet) == 14, "struct Ethernet is incorrect");
static_assert(std::is_pod<Vlan>::value, "not a POD type");
static_assert(sizeof(Vlan) == 4, "struct Vlan is incorrectly sized");

}  // namespace headers
}  // namespace xlb

#endif  // XLB_HEADERS_ETHER_H
