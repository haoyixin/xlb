#pragma once

#include "ports/common.h"

namespace xlb::ports {

// This driver binds a port to a device using DPDK.
class PMD final : public Port {
public:
  static const uint16_t kDpdkPortUnknown = RTE_MAX_ETHPORTS;

  PMD();
  ~PMD() override;

  inline uint16_t Recv(uint16_t qid, Packet **pkts, uint16_t cnt) override;
  inline uint16_t Send(uint16_t qid, Packet **pkts, uint16_t cnt) override;

  struct Status Status() override;

  const struct rte_eth_dev_info *dev_info() { return &dev_info_; }
  uint16_t dpdk_port_id() { return dpdk_port_id_; }

private:
  using M = utils::Metric<TS("xlb_ports"), TS("pmd")>;
  // The DPDK port ID number (set after binding).
  uint16_t dpdk_port_id_;
  struct rte_eth_dev_info dev_info_;
};

}  // namespace xlb::ports
