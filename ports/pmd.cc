#include "ports/pmd.h"

#include <headers/ip.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>

#include "utils/range.h"

#include "config.h"
#include "headers/ether.h"
#include "utils/format.h"

namespace xlb {
namespace ports {

PMD::PMD(std::string &&name)
    : Port(name), dpdk_port_id_(DPDK_PORT_UNKNOWN), node_(SOCKET_ID_ANY) {
  InitDriver();
  Init();
}

PMD::~PMD() { DeInit(); }

static const struct rte_eth_conf
default_eth_conf(struct rte_eth_dev_info &dev_info) {
  struct rte_eth_conf ret = {0};

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

void filter_add(uint16_t port_id, const std::string &dst_ip, queue_t qid) {
  CHECK(!rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_NTUPLE));
  // TODO: support RTE_ETH_FILTER_FDIR

  struct rte_eth_ntuple_filter ntuple = {0};
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

void PMD::InitDriver() {
  dpdk_port_t num_dpdk_ports = rte_eth_dev_count();

  CHECK_NE(num_dpdk_ports, 0);

  LOG(INFO) << static_cast<int>(num_dpdk_ports)
            << " nics have been recognized:";

  for (dpdk_port_t i = 0; i < num_dpdk_ports; i++) {
    struct rte_eth_dev_info dev_info;
    std::string pci_info;
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

// Find a port attached to DPDK by its PCI address.
// returns true and sets *ret_port_id to the port_id of the port at PCI address
// "pci" if it is valid and available.
static bool find_dpdk_port_by_pci_addr(const std::string &pci,
                                       dpdk_port_t *ret_port_id) {
  dpdk_port_t port_id = 0;
  struct rte_eth_dev_info info = {0};
  struct rte_pci_addr addr = {0};

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

void PMD::Init() {
  dpdk_port_t ret_port_id = DPDK_PORT_UNKNOWN;

  uint8_t num_q = CONFIG.worker_cores.size();

  CHECK(find_dpdk_port_by_pci_addr(CONFIG.nic.pci_address, &ret_port_id));

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(ret_port_id, &dev_info);

  struct rte_eth_conf eth_conf = default_eth_conf(dev_info);
  CHECK(!rte_eth_dev_configure(ret_port_id, num_q, num_q, &eth_conf));

  rte_eth_promiscuous_enable(ret_port_id);

  int sid = rte_eth_dev_socket_id(ret_port_id);

  /* Use defaut rx/tx configuration as provided by PMD ports,
   * maybe need minor tweaks */

  struct rte_eth_rxconf eth_rxconf = dev_info.default_rxconf;
  struct rte_eth_txconf eth_txconf = dev_info.default_txconf;

  eth_txconf.offloads = eth_conf.txmode.offloads;
  eth_rxconf.offloads = eth_conf.rxmode.offloads;

  // TODO: configurable queue size
  for (auto i : utils::range(0, num_q))
    CHECK(!rte_eth_tx_queue_setup(ret_port_id, i, dev_info.tx_desc_lim.nb_max,
                                  sid, &eth_txconf));

  for (auto i : utils::range(0, num_q))
    CHECK(!rte_eth_rx_queue_setup(ret_port_id, i, dev_info.rx_desc_lim.nb_max,
                                  sid, &eth_rxconf,
                                  PacketPool::GetPool(sid)->pool()));

  CHECK(!rte_eth_dev_set_vlan_offload(ret_port_id, ETH_VLAN_STRIP_OFFLOAD));

  for (auto i : utils::range(0, CONFIG.nic.local_ips.size())) {
    filter_add(ret_port_id, CONFIG.nic.local_ips[i],
               i % CONFIG.worker_cores.size());
  }

  CHECK(!rte_eth_dev_start(ret_port_id));

  dpdk_port_id_ = ret_port_id;
  node_ = sid;

  rte_eth_macaddr_get(dpdk_port_id_,
                      reinterpret_cast<ether_addr *>(conf_.mac_addr.bytes));
  CHECK(!rte_eth_dev_set_mtu(dpdk_port_id_, CONFIG.nic.mtu));

  conf_.mtu = CONFIG.nic.mtu;
  conf_.admin_up = true;

  // Reset hardware stat counters, as they may still contain previous data
  CHECK(!rte_eth_stats_reset(dpdk_port_id_));
}

void PMD::DeInit() { rte_eth_dev_stop(dpdk_port_id_); }

bool PMD::GetStats(Port::Stats &stats) {
  struct rte_eth_stats phy_stats = {0};

  int ret = rte_eth_stats_get(dpdk_port_id_, &phy_stats);
  if (ret < 0) {
    LOG(ERROR) << "rte_eth_stats_get(" << static_cast<int>(dpdk_port_id_)
               << ") failed: " << rte_strerror(-ret);
    return false;
  }

  VLOG(1) << utils::Format(
      "PMD port %d: ipackets %" PRIu64 " opackets %" PRIu64 " ibytes %" PRIu64
      " obytes %" PRIu64 " imissed %" PRIu64 " ierrors %" PRIu64
      " oerrors %" PRIu64 " rx_nombuf %" PRIu64,
      dpdk_port_id_, phy_stats.ipackets, phy_stats.opackets, phy_stats.ibytes,
      phy_stats.obytes, phy_stats.imissed, phy_stats.ierrors, phy_stats.oerrors,
      phy_stats.rx_nombuf);

  stats.inc.errors = phy_stats.ierrors + phy_stats.rx_nombuf;
  stats.inc.dropped = phy_stats.imissed;
  stats.inc.packets = phy_stats.ipackets;
  stats.inc.bytes = phy_stats.ibytes;

  stats.out.errors = phy_stats.oerrors;
  stats.out.packets = phy_stats.opackets;
  stats.out.bytes = phy_stats.obytes;

  packet_dir_t dir = PACKET_DIR_OUT;
  uint64_t odroped = 0;

  for (queue_t qid = 0; qid < CONFIG.worker_cores.size(); qid++)
    odroped += queue_stats[dir][qid].dropped;

  stats.out.dropped = odroped;

  return true;
  // TODO: status per queue
}

int PMD::RecvPackets(queue_t qid, Packet **pkts, int cnt) {
  return rte_eth_rx_burst(dpdk_port_id_, qid, (struct rte_mbuf **)pkts, cnt);
}

int PMD::SendPackets(queue_t qid, Packet **pkts, int cnt) {
  int sent = rte_eth_tx_burst(dpdk_port_id_, qid,
                              reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
  queue_stats[PACKET_DIR_OUT][qid].dropped += (cnt - sent);
  return sent;
}

Port::LinkStatus PMD::GetLinkStatus() {
  struct rte_eth_link status;
  // rte_eth_link_get() may block up to 9 seconds, so use _nowait() variant.
  rte_eth_link_get_nowait(dpdk_port_id_, &status);

  return LinkStatus{.speed = status.link_speed,
                    .full_duplex = static_cast<bool>(status.link_duplex),
                    .autoneg = static_cast<bool>(status.link_autoneg),
                    .link_up = static_cast<bool>(status.link_status)};
}

} // namespace ports
} // namespace xlb

DEFINE_PORT(PMD);
