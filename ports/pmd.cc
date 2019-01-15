#include "ports/pmd.h"

#include <rte_ethdev_pci.h>

#include "utils/boost.h"
#include "utils/format.h"
#include "utils/range.h"

#include "headers/ether.h"
#include "headers/ip.h"

#include "config.h"

namespace xlb {
namespace ports {

namespace {

const struct rte_eth_conf default_eth_conf(struct rte_eth_dev_info &dev_info) {
  struct rte_eth_conf ret {};

  // TODO: support software offload
  CHECK_NE(dev_info.rx_offload_capa & DEV_RX_OFFLOAD_IPV4_CKSUM, 0);
  CHECK_NE(dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TCP_CKSUM, 0);
  CHECK_NE(dev_info.rx_offload_capa & DEV_RX_OFFLOAD_VLAN_STRIP, 0);

  CHECK_NE(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM, 0);
  CHECK_NE(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM, 0);

  ret.rxmode.mq_mode = ETH_MQ_RX_RSS;
  ret.rxmode.offloads = (DEV_RX_OFFLOAD_IPV4_CKSUM | DEV_RX_OFFLOAD_TCP_CKSUM |
                         DEV_RX_OFFLOAD_CRC_STRIP | DEV_RX_OFFLOAD_VLAN_STRIP);
  ret.rxmode.ignore_offload_bitfield = 1;
  ret.txmode.offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM);

  ret.link_speeds = ETH_LINK_SPEED_AUTONEG;
  ret.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_PROTO_MASK;

  ret.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
  ret.fdir_conf.mask.ipv4_mask.src_ip = UINT32_MAX;
  ret.fdir_conf.mask.ipv4_mask.dst_ip = UINT32_MAX;
  ret.fdir_conf.drop_queue = UINT8_MAX;
  ret.fdir_conf.status = RTE_FDIR_REPORT_STATUS;

  return ret;
}

void filter_add(uint16_t port_id, const std::string &dst_ip, uint16_t qid) {
  CHECK(!rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_NTUPLE));
  // TODO: support RTE_ETH_FILTER_FDIR

  struct rte_eth_ntuple_filter ntuple {};
  utils::be32_t dst;
  headers::ParseIpv4Address(dst_ip, &dst);

  ntuple.flags = RTE_5TUPLE_FLAGS;
  ntuple.dst_ip = dst.raw_value();
  ntuple.dst_ip_mask = UINT32_MAX;
  ntuple.priority = 1;
  ntuple.queue = qid;

  CHECK(!rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_NTUPLE,
                                 RTE_ETH_FILTER_ADD, &ntuple));
}

// Find a port attached to DPDK by its PCI address.
// returns true and sets *ret_port_id to the port_id of the port at PCI address
// "pci" if it is valid and available.
bool find_dpdk_port_by_pci_addr(const std::string &pci, uint16_t *ret_port_id) {
  uint16_t port_id{};
  struct rte_eth_dev_info info {};
  struct rte_pci_addr addr {};

  rte_pci_addr_parse(pci.c_str(), &addr);

  RTE_ETH_FOREACH_DEV(port_id) {
    rte_eth_dev_info_get(port_id, &info);
    if (info.pci_dev &&
        rte_eal_compare_pci_addr(&addr, &info.pci_dev->addr) == 0) {
      *ret_port_id = port_id;
      return true;
    }
  }
  return false;
}

void init_driver() {
  uint16_t num_dpdk_ports = rte_eth_dev_count();

  CHECK_NE(num_dpdk_ports, 0);

  LOG(INFO) << static_cast<int>(num_dpdk_ports)
            << " nics have been recognized:";

  for (auto i : utils::irange((uint16_t)0, num_dpdk_ports)) {
    struct rte_eth_dev_info dev_info {};
    std::string pci_info{};
    int numa_node = -1;

    headers::Ethernet::Address mac_addr{};

    rte_eth_dev_info_get(i, &dev_info);

    if (dev_info.pci_dev) {
      pci_info = utils::Format(
          "%08x:%02hhx:%02hhx.%02hhx %04hx:%04hx  ",
          dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus,
          dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function,
          dev_info.pci_dev->id.vendor_id, dev_info.pci_dev->id.device_id);
    }

    numa_node = rte_eth_dev_socket_id(static_cast<int>(i));
    rte_eth_macaddr_get(i, reinterpret_cast<ether_addr *>(mac_addr.bytes));

    LOG(INFO) << "DPDK port_id " << static_cast<int>(i) << " ("
              << dev_info.driver_name << ")   RXQ " << dev_info.max_rx_queues
              << " TXQ " << dev_info.max_tx_queues << "  "
              << mac_addr.ToString() << "  " << pci_info << " numa_node "
              << numa_node;
  }
}

} // namespace

PMD::PMD() : Port(), dpdk_port_id_(kDpdkPortUnknown) {
  M::Expose<TSTR("rx_packets"), TSTR("tx_packets"), TSTR("tx_dropped")>;

  init_driver();

  uint16_t num_q = CONFIG.worker_cores.size();

  CHECK(find_dpdk_port_by_pci_addr(CONFIG.nic.pci_address, &dpdk_port_id_));

  struct rte_eth_dev_info dev_info {};
  rte_eth_dev_info_get(dpdk_port_id_, &dev_info);

  struct rte_eth_conf eth_conf = default_eth_conf(dev_info);
  CHECK(!rte_eth_dev_configure(dpdk_port_id_, num_q, num_q, &eth_conf));

  rte_eth_promiscuous_enable(dpdk_port_id_);

  int sid = CONFIG.nic.socket;

  /* Use defaut rx/tx configuration as provided by PMD ports,
   * maybe need minor tweaks */

  struct rte_eth_rxconf eth_rxconf = dev_info.default_rxconf;
  struct rte_eth_txconf eth_txconf = dev_info.default_txconf;

  eth_txconf.offloads = eth_conf.txmode.offloads;
  eth_rxconf.offloads = eth_conf.rxmode.offloads;

  // TODO: configurable queue size
  for (auto i : utils::range(0, num_q))
    CHECK(!rte_eth_tx_queue_setup(dpdk_port_id_, i, dev_info.tx_desc_lim.nb_max,
                                  sid, &eth_txconf));

  for (auto i : utils::range(0, num_q))
    CHECK(!rte_eth_rx_queue_setup(
        dpdk_port_id_, i, dev_info.rx_desc_lim.nb_max, sid, &eth_rxconf,
        utils::Singleton<PacketPool>::instance().pool()));

  CHECK(!rte_eth_dev_set_vlan_offload(dpdk_port_id_, ETH_VLAN_STRIP_OFFLOAD));

  for (auto i : utils::range(0, CONFIG.nic.local_ips.size())) {
    filter_add(dpdk_port_id_, CONFIG.nic.local_ips[i],
               i % CONFIG.worker_cores.size());
  }

  CHECK(!rte_eth_dev_start(dpdk_port_id_));
  CHECK_NE(dpdk_port_id_, kDpdkPortUnknown);

  rte_eth_macaddr_get(dpdk_port_id_,
                      reinterpret_cast<ether_addr *>(conf_.addr.bytes));
  CHECK(!rte_eth_dev_set_mtu(dpdk_port_id_, CONFIG.nic.mtu));

  conf_.mtu = CONFIG.nic.mtu;

  // Reset hardware stat counters, as they may still contain previous data
  CHECK(!rte_eth_stats_reset(dpdk_port_id_));
}

PMD::~PMD() { rte_eth_dev_stop(dpdk_port_id_); }

struct Port::Status PMD::Status() {
  struct rte_eth_link dpdk_status;
  // rte_eth_link_get() may block up to 9 seconds, so use _nowait() variant.
  rte_eth_link_get_nowait(dpdk_port_id_, &dpdk_status);

  struct Port::Status port_status;

  port_status.speed = dpdk_status.link_speed;
  port_status.full_duplex = static_cast<bool>(dpdk_status.link_duplex);
  port_status.autoneg = static_cast<bool>(dpdk_status.link_autoneg);
  port_status.up = static_cast<bool>(dpdk_status.link_status);

  return port_status;
}

} // namespace ports
} // namespace xlb

// DEFINE_PORT(PMD);
