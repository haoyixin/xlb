#pragma once

#include "headers/common.h"

namespace xlb::headers {

// return false if string -> be32_t conversion failed (*addr is unmodified)
bool ParseIpv4Address(const std::string &str, be32_t *addr);

// be32 -> string
std::string ToIpv4Address(const be32_t &addr);

// An IPv4 header definition loosely based on the BSD version.
struct [[gnu::packed]] Ipv4 {
  enum Flag : uint16_t {
    kMF = 1 << 13,  // More fragments
    kDF = 1 << 14,  // Do not fragment
  };

  static constexpr uint16_t kOffsetMask = 0x1fff;

  enum Proto : uint8_t {
    kIcmp = 1,
    kIgmp = 2,
    kIpIp = 4,  // IPv4-in-IPv4
    kTcp = 6,
    kUdp = 17,
    kIpv6 = 41,  // IPv6-in-IPv4
    kGre = 47,
    kEsp = 50,  // IPsec ESP (Encapsulating Security Payload)
    kAh = 51,   // IPsec AH (Authentication Header)
    kSctp = 132,
    kUdpLite = 136,
    kMpls = 137,  // MPLS-in-IPv4
    kRaw = 255,
  };

#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint8_t header_length : 4;  // Header length.
  uint8_t version : 4;        // Version.
#elif __BYTE_ORDER == __BIG_ENDIAN
  uint8_t version : 4;        // Version.
  uint8_t header_length : 4;  // Header length.
#else
#error __BYTE_ORDER must be defined.
#endif
  uint8_t type_of_service;  // Type of service.
  be16_t length;            // Length.
  be16_t id;                // ID.
  union {
    be16_t fragment_offset;  // Fragment offset.
#if __BYTE_ORDER == __LITTLE_ENDIAN
    struct {
      uint8_t : 8;
      bool : 1, df : 1, mf : 1, : 5;
    };
#elif __BYTE_ORDER == __BIG_ENDIAN
    struct {
      uint8_t : 1, df : 1, mf : 1, : 5;
      uint8_t : 8;
    };
#endif
  };
  uint8_t ttl;        // Time to live.
  uint8_t protocol;   // Protocol.
  uint16_t checksum;  // Checksum.
  be32_t src;         // Source address.
  be32_t dst;         // Destination address.
};

static_assert(std::is_pod<Ipv4>::value, "not a POD type");
static_assert(sizeof(Ipv4) == 20, "struct Ipv4 is incorrect");

/*
struct Ipv4Prefix {
  // Implicit default constructor is not allowed
  Ipv4Prefix() = delete;

  // Construct Ipv4Prefix from a string like "192.168.0.1/24"
  explicit Ipv4Prefix(const std::string &prefix);

  // Returns true if ip is within the range of Ipv4Prefix
  bool Match(const be32_t &ip) const { return (addr & mask) == (ip & mask); }

  // Returns the prefix length
  uint32_t prefix_length() const {
    uint32_t mask_val = mask.value();
    if (mask_val == 0) {
      return 0;
    } else {
      return 32 - __builtin_ctz(mask_val);
    }
  }

  be32_t addr{};
  be32_t mask{};
};
 */

}  // namespace xlb::headers
