#ifndef XLB_PORTS_PMD_H
#define XLB_PORTS_PMD_H

#include <string>

#include <rte_ethdev.h>

#include "utils/metric.h"

#include "module.h"
#include "port.h"

namespace xlb {
namespace ports {

// This driver binds a port to a device using DPDK.
class PMD final : public Port {
public:
  static const uint16_t kDpdkPortUnknown = RTE_MAX_ETHPORTS;

  PMD();
  ~PMD() override;

  // recv and send should be inline
  uint16_t Recv(uint16_t qid, Packet **pkts, uint16_t cnt) override {
    auto recv =
        rte_eth_rx_burst(dpdk_port_id_, qid, (struct rte_mbuf **)pkts, cnt);
    M::Adder<TSTR("rx_packets")>() << recv;
    return recv;
  }

  uint16_t Send(uint16_t qid, Packet **pkts, uint16_t cnt) override {
    auto sent = rte_eth_tx_burst(
        dpdk_port_id_, qid, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
    M::Adder<TSTR("tx_dropped")>() << (cnt - sent);
    M::Adder<TSTR("tx_packets")>() << sent;
    return sent;
  }

  struct Status Status() override;

private:
  using M = utils::Metric<TSTR("xlb_ports"), TSTR("pmd")>;
  // The DPDK port ID number (set after binding).
  uint16_t dpdk_port_id_;
};

} // namespace ports
} // namespace xlb

#endif // XLB_PORTS_PMD_H
