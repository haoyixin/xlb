#ifndef XLB_HEADERS_ARP_H
#define XLB_HEADERS_ARP_H

#include "headers/ether.h"
#include "headers/ip.h"
#include "utils/endian.h"

#include "headers/common.h"

namespace xlb {
namespace headers {

// A basic ARP header definition
struct [[gnu::packed]] Arp {
  // Ethernet hardware format for hrd
  enum HardwareAddress : uint16_t {
    kEthernet = 1,
  };

  enum Opcode : uint16_t {
    kRequest = 1,
    kReply = 2,
    kRevRequest = 3,
    kRevReply = 4,
    kInvRequest = 8,
    kInvReply = 9,
  };

  be16_t hw_addr;            // format of hardware address (hrd)
  be16_t proto_addr;         // format of protocol address (pro)
  uint8_t hw_addr_length;    // length of hardware address (hln)
  uint8_t proto_addr_length; // length of protocol address (pln)
  be16_t opcode;             // ARP opcode (command) (op)

  // ARP Data
  Ethernet::Address sender_hw_addr; // sender hardware address (sha)
  be32_t sender_ip_addr;            // sender IP address (sip)
  Ethernet::Address target_hw_addr; // target hardware address (tha)
  be32_t target_ip_addr;            // target IP address (tip)
};

static_assert(sizeof(Arp) == 28, "struct Arp size is incorrect");

} // namespace headers
} // namespace xlb

#endif // XLB_HEADERS_ARP_H
