#pragma once

#include "headers/common.h"

namespace xlb::headers {

// A basic TCP header definition loosely based on the BSD version.
struct [[gnu::packed]] Tcp {
  //  enum Flag : uint8_t {
  //    kFin = 0x01,
  //    kSyn = 0x02,
  //    kRst = 0x04,
  //    kPsh = 0x08,
  //    kAck = 0x10,
  //    kUrg = 0x20,
  //  };

  be16_t src_port;  // Source port.
  be16_t dst_port;  // Destination port.
  be32_t seq_num;   // Sequence number.
  be32_t ack_num;   // Acknowledgement number.
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint8_t reserved : 4;  // Unused reserved bits.
  uint8_t offset : 4;    // Data offset.
#elif __BYTE_ORDER == __BIG_ENDIAN
  uint8_t offset : 4;    // Data offset.
  uint8_t reserved : 4;  // Unused reserved bits.
#else
#error __BYTE_ORDER must be defined.
#endif
  union {
    uint8_t flags;
    bool fin : 1, syn : 1, rst : 1, psh : 1, ack : 1, urg : 1, ece : 1, cwr : 1;
  };
  be16_t window;      // Receive window.
  uint16_t checksum;  // Checksum.
  be16_t urgent_ptr;  // Urgent pointer.
};

struct [[gnu::packed]] Toa {
  uint8_t code;
  uint8_t size;
  be16_t port;
  be32_t addr;
};

static_assert(std::is_pod<Tcp>::value, "not a POD type");
static_assert(std::is_pod<Toa>::value, "not a POD type");
static_assert(sizeof(Tcp) == 20, "struct Tcp is incorrect");

}  // namespace xlb::headers
