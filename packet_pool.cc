#include "packet_pool.h"

#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_mempool.h>

#include "dpdk.h"
#include "opts.h"

#include "config.h"
#include "utils/numa.h"

namespace xlb {
namespace {

struct PoolPrivate {
  rte_pktmbuf_pool_private dpdk_priv;
  PacketPool *owner;
};

// callback function for each packet
void InitPacket(rte_mempool *mp, void *, void *mbuf, unsigned index) {
  rte_pktmbuf_init(mp, nullptr, mbuf, index);

  auto *pkt = static_cast<Packet *>(mbuf);
  pkt->set_vaddr(pkt);
  pkt->set_paddr(rte_mempool_virt2iova(pkt));
}

} // namespace

void PacketPool::CreatePools(size_t capacity) {
  InitDpdk(CONFIG.hugepage);

  rte_dump_physmem_layout(stdout);

  // TODO: only support one numa node now

  int sid = CONFIG.nic.socket;
  LOG(INFO) << "Creating DpdkPacketPool for " << capacity << " packets on node "
            << sid;
  pools_[sid] = new DpdkPacketPool(capacity, sid);

  CHECK(pools_[sid]) << "Packet pool allocation on node " << sid << " failed!";
}

PacketPool::PacketPool(size_t capacity, int socket_id) {
  static int next_id_;
  name_ = "PacketPool" + std::to_string(next_id_++);

  LOG(INFO) << name_ << " requests for " << capacity << " packets";

  pool_ = rte_mempool_create_empty(name_.c_str(), capacity, sizeof(Packet),
                                   capacity > 1024 ? kMaxCacheSize : 0,
                                   sizeof(PoolPrivate), socket_id, 0);
  if (!pool_) {
    LOG(FATAL) << "rte_mempool_create() failed: " << rte_strerror(rte_errno)
               << " (rte_errno=" << rte_errno << ")";
  }

  int ret = rte_mempool_set_ops_byname(pool_, "ring_mp_mc", NULL);
  if (ret < 0) {
    LOG(FATAL) << "rte_mempool_set_ops_byname() returned " << ret;
  }
}

PacketPool::~PacketPool() { rte_mempool_free(pool_); }

void PacketPool::PostPopulate() {
  PoolPrivate priv = {
      .dpdk_priv = {.mbuf_data_room_size = XBUF_HEADROOM + XBUF_DATA,
                    .mbuf_priv_size = XBUF_RESERVE},
      .owner = this};

  rte_pktmbuf_pool_init(pool_, &priv.dpdk_priv);
  rte_mempool_obj_iter(pool_, InitPacket, nullptr);

  LOG(INFO) << name_ << " has been created with " << Capacity() << " packets";
  if (Capacity() == 0) {
    LOG(FATAL) << name_ << " has no packets allocated\n"
               << "Troubleshooting:\n"
               << "  - Check 'ulimit -l'\n"
               << "  - Do you have enough memory on the machine?\n"
               << "  - Maybe memory is too fragmented. Try rebooting.\n";
  }
}

DpdkPacketPool::DpdkPacketPool(size_t capacity, int socket_id)
    : PacketPool(capacity, socket_id) {
  int ret = rte_mempool_populate_default(pool_);
  if (ret < static_cast<ssize_t>(pool_->size)) {
    LOG(WARNING) << "rte_mempool_populate_default() returned " << ret
                 << " (rte_errno=" << rte_errno << ", "
                 << rte_strerror(rte_errno) << ")";
  }

  PostPopulate();
}

} // namespace xlb
