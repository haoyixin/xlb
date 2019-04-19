#pragma once

#include "headers/common.h"

namespace xlb::headers {

struct [[gnu::packed]] Ethernet {
  struct [[gnu::packed]] Address {
    Address() = default;
    Address(const uint8_t *addr) { utils::CopyInlined(bytes, addr, kSize); }
    Address(const std::string &str);

    static constexpr size_t kSize = 6;

    // Returns false if the format is incorrect.
    //   (in that case, the content of parsed is undefined.)
    bool FromString(const std::string &str);

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

    friend std::ostream &operator<<(std::ostream &os, const Address &addr) {
      os << addr.ToString();
      return os;
    }
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

}  // namespace xlb::headers
