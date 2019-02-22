#include "ports/kni.h"

#include "utils/iface.h"
#include "utils/singleton.h"

#include "ports/pmd.h"

#include "packet_pool.h"
#include "xbuf_layout.h"

namespace xlb::ports {

namespace {

void init_driver() { rte_kni_init(1); }

}  // namespace

template <typename... Args>
KNI::KNI(Args &&... args) : Port() {
  init_driver();

  auto pmd_port = utils::Singleton<PMD>::instance(std::forward<Args>(args)...);

  struct rte_kni_conf kni_conf = {.name = "xlb",
                                  .core_id = 0,
                                  .group_id = pmd_port.dpdk_port_id(),
                                  .mbuf_size = XBUF_SIZE,
                                  .addr = pmd_port.dev_info()->pci_dev->addr,
                                  .id = pmd_port.dev_info()->pci_dev->id,
                                  .force_bind = 0};

  kni_ = rte_kni_alloc(utils::Singleton<PacketPool>::instance().pool(),
                       &kni_conf, nullptr);
  CHECK_NOTNULL(kni_);

  conf_ = pmd_port.conf();
  CHECK(utils::SetHwAddr("xlb", conf_.addr));

  CHECK((M::Expose<TS("rx_packets"), TS("rx_bytes"), TS("tx_packets"),
                   TS("tx_bytes"), TS("tx_dropped")>));
}

uint16_t KNI::Recv(uint16_t qid, Packet **pkts, uint16_t cnt) {
  auto recv =
      rte_kni_rx_burst(kni_, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
  M::Adder<TS("rx_packets")>() << recv;

  for (auto i : utils::irange(recv))
    M::Adder<TS("rx_bytes")>() << pkts[i]->data_len();

  return recv;
}

uint16_t KNI::Send(uint16_t qid, Packet **pkts, uint16_t cnt) {
  M::Adder<TS("tx_packets")>() << cnt;

  for (auto i : utils::irange(cnt))
    M::Adder<TS("tx_bytes")>() << pkts[i]->data_len();

  auto sent =
      rte_kni_tx_burst(kni_, reinterpret_cast<struct rte_mbuf **>(pkts), cnt);
  M::Adder<TS("tx_dropped")>() << (cnt - sent);
  return sent;
}

KNI::~KNI() { rte_kni_release(kni_); }

}  // namespace xlb::ports
