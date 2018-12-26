#ifndef XLB_PORT_H
#define XLB_PORT_H

#include <string>

#include "headers/ether.h"
#include "packet.h"
#include "packet_batch.h"
#include "utils/common.h"

namespace xlb {

typedef uint8_t queue_t;

class Port {
public:
  /* The term RX/TX could be very confusing for a virtual port.
   * Instead, we use the "incoming/outgoizng" convention:
   * - incoming: outside -> XLB
   * - outgoing: XLB -> outside */
  typedef enum { INC = 0, OUT = 1, DIRS } Direction;

  struct QueueCounters {
    uint64_t packets;
    uint64_t dropped;
    uint64_t errors;
    uint64_t bytes; // It doesn't include Ethernet overhead
  };

  struct LinkStatus {
    uint32_t speed;   // speed in mbps: 1000, 40000, etc. 0 for vports
    bool full_duplex; // full-duplex enabled?
    bool autoneg;     // auto-negotiated speed and duplex?
    bool link_up;     // link up?
  };

  struct Conf {
    headers::Ethernet::Address mac_addr;
    uint16_t mtu;
  };

  struct Counters {
    QueueCounters inc;
    QueueCounters out;
  };

  static const uint32_t kDefaultMtu = 1500;
  static const uint32_t kMaxQueues = 32;

  // overide this section to create a new driver -----------------------------

  virtual ~Port() {}

  virtual int RecvPackets(queue_t qid, Packet **pkts, int cnt) = 0;
  virtual int SendPackets(queue_t qid, Packet **pkts, int cnt) = 0;

  virtual LinkStatus GetLinkStatus() = 0;

  // return false if failed
  virtual bool GetStats(Port::Counters &stats) = 0;

  // TODO: reset status

  const std::string &name() const { return name_; }
  const Conf &conf() const { return conf_; }

protected:
  explicit Port(std::string &name) : name_(name), conf_(), queue_counters_() {}

  // Current configuration
  Conf conf_;

private:
  std::string name_; // The name of this port instance.

  DISALLOW_COPY_AND_ASSIGN(Port);

public:
  // TODO: more encapsulated
  struct QueueCounters queue_counters_[DIRS][kMaxQueues];
};

} // namespace xlb

// TODO: support multi port, builder ?

#define DEFINE_PORT(_PORT) xlb::ports::_PORT *PORTS_##_PORT

#define DECLARE_PORT(_PORT) extern xlb::ports::_PORT *PORTS_##_PORT

#define PORT_INIT(_PORT, ...)                                                  \
  PORTS_##_PORT = new xlb::ports::_PORT(#_PORT, ##__VA_ARGS__)

#endif // XLB_PORT_H
