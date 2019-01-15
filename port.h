#ifndef XLB_PORT_H
#define XLB_PORT_H

#include <memory>
#include <string>

#include "headers/ether.h"

#include "utils/boost.h"
#include "utils/common.h"
#include "utils/format.h"
#include "utils/metric.h"

#include "packet.h"
#include "packet_batch.h"

namespace xlb {

class Port {
public:
  struct Status {
    uint32_t speed;   // speed in mbps: 1000, 40000, etc. 0 for vports
    bool full_duplex; // full-duplex enabled?
    bool autoneg;     // auto-negotiated speed and duplex?
    bool up;          // link up?
  };

  struct Conf {
    headers::Ethernet::Address addr;
    uint16_t mtu;
  };

  static const uint32_t kDefaultMtu = 1500;
  static const uint32_t kMaxQueues = 32;

  virtual ~Port() = default;

  virtual uint16_t Recv(uint16_t qid, Packet **pkts, uint16_t cnt) = 0;
  virtual uint16_t Send(uint16_t qid, Packet **pkts, uint16_t cnt) = 0;

  virtual struct Status Status() = 0;

  const Conf &conf() const { return conf_; }

protected:
  explicit Port() : conf_() {}

  // Current configuration
  Conf conf_;

private:
  DISALLOW_COPY_AND_ASSIGN(Port);
};

} // namespace xlb

#endif // XLB_PORT_H
