#ifndef XLB_PORTS_PMD_H
#define XLB_PORTS_PMD_H

#include <string>

#include <rte_config.h>
#include <rte_errno.h>
#include <rte_ethdev.h>

#include "module.h"
#include "port.h"

#define DPDK_PORT_UNKNOWN RTE_MAX_ETHPORTS

namespace xlb {
namespace ports {

typedef uint16_t dpdk_port_t;

// This driver binds a port to a device using DPDK.
class PMDPort final : public Port {
public:
  PMDPort(std::string &name);

  bool GetStats(Port::Stats &stats) override;

  int RecvPackets(queue_t qid, Packet **pkts, int cnt) override;

  int SendPackets(queue_t qid, Packet **pkts, int cnt) override;

  uint64_t GetFlags() const override {
    return DRIVER_FLAG_SELF_INC_STATS | DRIVER_FLAG_SELF_OUT_STATS;
  }

  LinkStatus GetLinkStatus() override;

  int UpdateConf(const Conf &conf) override;

  int GetNode() const { return node_; }

private:
  // The DPDK port ID number (set after binding).
  dpdk_port_t dpdk_port_id_;

  // The NUMA node to which device is attached
  int node_;
};

} // namespace ports
} // namespace xlb

#endif // XLB_PORTS_PMD_H
