#ifndef XLB_PORTS_PMD_H
#define XLB_PORTS_PMD_H

#include <string>

#include <rte_config.h>
#include <rte_errno.h>
#include <rte_ethdev.h>

#include "module.h"
#include "port.h"

namespace xlb {
namespace ports {

typedef uint16_t dpdk_port_t;

// This driver binds a port to a device using DPDK.
class PMD final : public Port {
public:
  static const int kDpdkPortUnknown = RTE_MAX_ETHPORTS;

  PMD(std::string &&name);
  ~PMD();

  bool GetStats(Port::Counters &stats) override;

  // This should be inline
  size_t RecvPackets(uint16_t qid, Packet **pkts, int cnt) override {
    return rte_eth_rx_burst(dpdk_port_id_, qid, (struct rte_mbuf **)pkts, cnt);
  }

  size_t SendPackets(uint16_t qid, Packet **pkts, int cnt) override {
    int sent = rte_eth_tx_burst(
        dpdk_port_id_, qid, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
    queue_counters_[OUT][qid].dropped += (cnt - sent);
    return sent;
  }

  LinkStatus GetLinkStatus() override;

private:
  // The DPDK port ID number (set after binding).
  dpdk_port_t dpdk_port_id_;

  static void InitDriver();
  void InitPort();
  void Destroy();
};

} // namespace ports
} // namespace xlb

#endif // XLB_PORTS_PMD_H
