#include "ports/pmd.h"

namespace xlb::ports {

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
  ret.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
  ret.rxmode.ignore_offload_bitfield = 1;
  ret.txmode.offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM);

  ret.link_speeds = ETH_LINK_SPEED_AUTONEG;
  ret.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_PROTO_MASK;

  //  ret.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
  //  ret.fdir_conf.mask.ipv4_mask.src_ip = UINT32_MAX;
  //  ret.fdir_conf.mask.ipv4_mask.dst_ip = UINT32_MAX;
  //  ret.fdir_conf.drop_queue = UINT8_MAX;
  //  ret.fdir_conf.status = RTE_FDIR_REPORT_STATUS;

  return ret;
}

/*
void filter_add(uint16_t port_id, const std::string &dst_ip, uint16_t qid) {
  CHECK(!rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_NTUPLE));

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
 */

void set_lip_affinity(uint16_t port_id, const utils::be32_t &dst,
                      uint16_t qid) {
  F_LOG(INFO) << "set local ip affinity: " << headers::ToIpv4Address(dst)
              << " to rxq: " << qid;

  //  utils::be32_t dst{};
  //  headers::ParseIpv4Address(dst_ip, &dst);

  rte_flow_attr attr{.group = 0, .priority = 0, .ingress = 1};

  ipv4_hdr ipv4{};
  ipv4.dst_addr = dst.raw_value();

  ipv4_hdr ipv4_mask{};
  ipv4_mask.dst_addr = 0xffffffff;

  rte_flow_item pattern[] = {{.type = RTE_FLOW_ITEM_TYPE_ETH},
                             {.type = RTE_FLOW_ITEM_TYPE_IPV4,
                              .spec = &ipv4,
                              .last = nullptr,
                              .mask = &ipv4_mask},
                             {.type = RTE_FLOW_ITEM_TYPE_END}};

  rte_flow_action actions[] = {
      {.type = RTE_FLOW_ACTION_TYPE_QUEUE, .conf = &qid},
      {.type = RTE_FLOW_ACTION_TYPE_END}};

  CHECK(!rte_flow_validate(port_id, &attr, pattern, actions, nullptr));
  CHECK(rte_flow_create(port_id, &attr, pattern, actions, nullptr));
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

  F_LOG(INFO) << static_cast<int>(num_dpdk_ports)
              << " nics have been recognized";

  for (auto i : utils::irange((uint16_t)0, num_dpdk_ports)) {
    struct rte_eth_dev_info dev_info {};
    std::string pci_info{};
    int numa_node = -1;

    headers::Ethernet::Address mac_addr{};

    rte_eth_dev_info_get(i, &dev_info);

    if (dev_info.pci_dev) {
      pci_info = utils::Format(
          "%08x:%02hhx:%02hhx.%02hhx %04hx:%04hx",
          dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus,
          dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function,
          dev_info.pci_dev->id.vendor_id, dev_info.pci_dev->id.device_id);
    }

    numa_node = rte_eth_dev_socket_id(static_cast<int>(i));
    rte_eth_macaddr_get(i, reinterpret_cast<ether_addr *>(mac_addr.bytes));

    F_LOG(INFO) << "port_id: " << static_cast<int>(i)
                << " driver: " << dev_info.driver_name
                << " RXQ: " << dev_info.max_rx_queues
                << " TXQ: " << dev_info.max_tx_queues
                << " mac_address: " << mac_addr.ToString()
                << " pci_address: " << pci_info << " numa_node: " << numa_node;
  }
}

}  // namespace

PMD::PMD() : Port(), dpdk_port_id_(kDpdkPortUnknown), dev_info_() {
  CHECK((M::Expose<TS("rx_packets"), TS("rx_bytes"), TS("tx_packets"),
                   TS("tx_bytes"), TS("tx_dropped")>));

  init_driver();

  uint16_t num_q = CONFIG.slave_cores.size();

  CHECK(find_dpdk_port_by_pci_addr(CONFIG.nic.pci_address, &dpdk_port_id_));

  rte_eth_dev_info_get(dpdk_port_id_, &dev_info_);

  struct rte_eth_conf eth_conf = default_eth_conf(dev_info_);
  CHECK(!rte_eth_dev_configure(dpdk_port_id_, num_q, num_q, &eth_conf));

  rte_eth_promiscuous_enable(dpdk_port_id_);

  int sid = CONFIG.nic.socket;

  /* Use defaut rx/tx configuration as provided by PMD ports,
   * maybe need minor tweaks */

  struct rte_eth_rxconf eth_rxconf = dev_info_.default_rxconf;
  struct rte_eth_txconf eth_txconf = dev_info_.default_txconf;

  eth_txconf.offloads = eth_conf.txmode.offloads;
  eth_txconf.txq_flags |= ETH_TXQ_FLAGS_IGNORE;

  eth_rxconf.offloads = eth_conf.rxmode.offloads;

  // TODO: configurable queue size
  for (auto i : utils::irange(num_q))
    CHECK(!rte_eth_tx_queue_setup(
        dpdk_port_id_, i, dev_info_.tx_desc_lim.nb_max, sid, &eth_txconf));

  for (auto i : utils::irange(num_q))
    CHECK(!rte_eth_rx_queue_setup(
        dpdk_port_id_, i, dev_info_.rx_desc_lim.nb_max, sid, &eth_rxconf,
        utils::Singleton<PacketPool>::instance().pool()));

  //  CHECK(!rte_eth_dev_set_vlan_offload(dpdk_port_id_,
  //  ETH_VLAN_STRIP_OFFLOAD));

  CHECK(!rte_flow_flush(dpdk_port_id_, nullptr));

  //  for (auto i : utils::irange(CONFIG.nic.local_ips.size())) {
  //    set_lip_affinity(dpdk_port_id_, CONFIG.nic.local_ips[i],
  //                     i % CONFIG.slave_cores.size());
  //  }

  for (auto &pair : CONFIG.slave_local_ips)
    set_lip_affinity(dpdk_port_id_, pair.second, pair.first);

  CHECK(!rte_eth_dev_start(dpdk_port_id_));
  CHECK_NE(dpdk_port_id_, kDpdkPortUnknown);

  rte_eth_macaddr_get(dpdk_port_id_,
                      reinterpret_cast<ether_addr *>(conf_.addr.bytes));

  // TODO: do it in Config::validate
  CONFIG.nic.mac_address = conf_.addr;

  CHECK(!rte_eth_dev_set_mtu(dpdk_port_id_, CONFIG.nic.mtu));

  conf_.mtu = CONFIG.nic.mtu;

  // Reset hardware stat counters, as they may still contain previous data
  CHECK(!rte_eth_stats_reset(dpdk_port_id_));

  struct rte_eth_link dpdk_status {};
  // Call the blocking version update cache once
  rte_eth_link_get(dpdk_port_id_, &dpdk_status);

  F_LOG(INFO) << "initialize successful, speed: " << dpdk_status.link_speed
              << " full-duplex: " << dpdk_status.link_duplex
              << " auto-neg: " << dpdk_status.link_autoneg
              << " up: " << dpdk_status.link_status;
}

PMD::~PMD() {
  F_LOG(INFO) << "stopping dpdk port: " << dpdk_port_id_;
  rte_eth_dev_stop(dpdk_port_id_);
}

struct Port::Status PMD::Status() {
  struct rte_eth_link dpdk_status {};
  // rte_eth_link_get() may block up to 9 seconds, so use _nowait() variant.
  rte_eth_link_get_nowait(dpdk_port_id_, &dpdk_status);

  struct Port::Status port_status {};

  port_status.speed = dpdk_status.link_speed;
  port_status.full_duplex = static_cast<bool>(dpdk_status.link_duplex);
  port_status.autoneg = static_cast<bool>(dpdk_status.link_autoneg);
  port_status.up = static_cast<bool>(dpdk_status.link_status);

  return port_status;
}

uint16_t PMD::Send(uint16_t qid, Packet **pkts, uint16_t cnt) {
  M::Adder<TS("tx_packets")>() << cnt;

  uint64_t bytes = 0;
  for (auto i : utils::irange(cnt)) bytes += pkts[i]->data_len();

  M::Adder<TS("tx_bytes")>() << bytes;

  auto sent = rte_eth_tx_burst(dpdk_port_id_, qid,
                               reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
  M::Adder<TS("tx_dropped")>() << (cnt - sent);
  return sent;
}

uint16_t PMD::Recv(uint16_t qid, Packet **pkts, uint16_t cnt) {
  auto recv = rte_eth_rx_burst(dpdk_port_id_, qid,
                               reinterpret_cast<struct rte_mbuf **>(pkts), cnt);

  if (recv > 0) {
    M::Adder<TS("rx_packets")>() << recv;

    uint64_t bytes = 0;
    for (auto i : utils::irange(recv)) bytes += pkts[i]->data_len();

    M::Adder<TS("rx_bytes")>() << bytes;
  }

  return recv;
}

}  // namespace xlb::ports

// DEFINE_PORT(PMD);
