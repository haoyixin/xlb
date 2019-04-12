#include "ports/kni.h"

#include <atomic>
#include <cstdlib>
#include <thread>

#include <rte_bus_pci.h>

#include "utils/iface.h"
#include "utils/singleton.h"

#include "ports/pmd.h"

#include "config.h"
#include "packet_pool.h"
#include "xbuf_layout.h"

namespace xlb::ports {

namespace {

void init_driver() { rte_kni_init(1); }

}  // namespace

KNI::KNI() : Port() {
  init_driver();

  auto &pmd_port = utils::Singleton<PMD>::instance();

  struct rte_kni_conf kni_conf = {"xlb",
                                  CONFIG.master_core,
                                  pmd_port.dpdk_port_id(),
                                  XBUF_SIZE,
                                  pmd_port.dev_info()->pci_dev->addr,
                                  pmd_port.dev_info()->pci_dev->id,
                                  1};

  struct rte_kni_ops kni_ops = {0, nullptr, nullptr};

  kni_ = rte_kni_alloc(utils::Singleton<PacketPool>::instance().pool(),
                       &kni_conf, &kni_ops);
  CHECK_NOTNULL(kni_);

  conf_ = pmd_port.conf();

  CHECK(utils::SetHwAddr("xlb", conf_.addr));

  CHECK((M::Expose<TS("rx_packets"), TS("rx_bytes"), TS("tx_packets"),
                   TS("tx_bytes"), TS("tx_dropped"), TS("req_failed")>));

  LOG(INFO) << "[KNI] initialize successful";
}

uint16_t KNI::Recv(uint16_t qid, Packet **pkts, uint16_t cnt) {
  if (rte_kni_handle_request(kni_) != 0) {
    LOG(ERROR) << "rte_kni_handle_request failed";
    M::Adder<TS("req_failed")>() << 1;
  }

  auto recv =
      rte_kni_rx_burst(kni_, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);

  if (recv > 0) {
    M::Adder<TS("rx_packets")>() << recv;

    uint64_t bytes = 0;
    for (auto i : utils::irange(recv)) bytes += pkts[i]->data_len();

    M::Adder<TS("rx_bytes")>() << bytes;
  }

  return recv;
}

uint16_t KNI::Send(uint16_t qid, Packet **pkts, uint16_t cnt) {
  M::Adder<TS("tx_packets")>() << cnt;

  uint64_t bytes = 0;
  for (auto i : utils::irange(cnt)) bytes += pkts[i]->data_len();

  M::Adder<TS("tx_bytes")>() << bytes;

  auto sent =
      rte_kni_tx_burst(kni_, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
  M::Adder<TS("tx_dropped")>() << (cnt - sent);
  return sent;
}

KNI::~KNI() {
  DLOG(INFO) << "Release kni interface";
  rte_kni_release(kni_);
}

}  // namespace xlb::ports
