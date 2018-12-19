#ifndef XLB_PORT_H
#define XLB_PORT_H

#include <glog/logging.h>
#include <gtest/gtest_prod.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "headers/ether.h"
#include "packet.h"
#include "packet_batch.h"
#include "utils/common.h"

#define MAX_QUEUES_PER_DIR 32 /* [0, 31] (for each RX/TX) */

#define MAX_QUEUE_SIZE 4096

#define ETH_ALEN 6

namespace xlb {

typedef uint8_t queue_t;
/* The term RX/TX could be very confusing for a virtual port.
 * Instead, we use the "incoming/outgoing" convention:
 * - incoming: outside -> XLB
 * - outgoing: XLB -> outside */
typedef enum {
  PACKET_DIR_INC = 0,
  PACKET_DIR_OUT = 1,
  PACKET_DIRS
} packet_dir_t;

class Port;

struct QueueStats {
  uint64_t packets;
  uint64_t dropped;
  uint64_t errors;
  uint64_t bytes; // It doesn't include Ethernet overhead
};

class Port {
public:
  struct LinkStatus {
    uint32_t speed;   // speed in mbps: 1000, 40000, etc. 0 for vports
    bool full_duplex; // full-duplex enabled?
    bool autoneg;     // auto-negotiated speed and duplex?
    bool link_up;     // link up?
  };

  struct Conf {
    headers::Ethernet::Address mac_addr;
    uint16_t mtu;
    bool admin_up;
  };

  struct Stats {
    QueueStats inc;
    QueueStats out;
  };

  static const uint32_t kDefaultMtu = 1500;

  // overide this section to create a new driver -----------------------------

  virtual ~Port() {}

  virtual int RecvPackets(queue_t qid, Packet **pkts, int cnt) = 0;
  virtual int SendPackets(queue_t qid, Packet **pkts, int cnt) = 0;

  virtual uint64_t GetFlags() const { return 0; }

  virtual LinkStatus GetLinkStatus() {
    return LinkStatus{
        .speed = 0,
        .full_duplex = true,
        .autoneg = true,
        .link_up = true,
    };
  }

  virtual int UpdateConf(const Conf &) { return -ENOTSUP; }

  // return false if failed
  virtual bool GetStats(Port::Stats &stats) = 0;

  // TODO: reset status

  const std::string &name() const { return name_; }
  const Conf &conf() const { return conf_; }

protected:
  explicit Port(std::string &name)
      : name_(name), conf_(), queue_stats() {}

  // Current configuration
  Conf conf_;

private:
  std::string name_; // The name of this port instance.

  DISALLOW_COPY_AND_ASSIGN(Port);

public:
  // TODO: more encapsulated
  struct QueueStats queue_stats[PACKET_DIRS][MAX_QUEUES_PER_DIR];
};

} // namespace xlb

// TODO: support multi port, builder ?

#define DEFINE_PORT(_PORT) xlb::ports::_PORT *PORTS_##_PORT

#define DECLARE_PORT(_PORT) extern xlb::ports::_PORT *PORTS_##_PORT

#define PORT_INIT(_PORT, ...)                                                  \
  PORTS_##_PORT = new xlb::ports::_PORT(#_PORT, ##__VA_ARGS__)

#endif // XLB_PORT_H
