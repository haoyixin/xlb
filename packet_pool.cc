#include "packet_pool.h"

#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_mempool.h>

#include "utils/numa.h"

#include "config.h"
#include "dpdk.h"

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

PacketPool::PacketPool(size_t capacity, int socket_id) {
  rte_dump_physmem_layout(stdout);

  // TODO: support multi numa node

  LOG(INFO) << "Creating PacketPool for " << capacity << " packets on node "
            << (socket_id == -1 ? 0 : socket_id);

  pool_ = rte_mempool_create_empty("PacketPool", capacity, sizeof(Packet),
                                   capacity > 1024 ? kMaxCacheSize : 0,
                                   sizeof(PoolPrivate), socket_id, 0);
  CHECK_NOTNULL(pool_);

  CHECK_EQ(rte_mempool_set_ops_byname(pool_, "ring_mp_mc", NULL), 0);

  CHECK_GE(rte_mempool_populate_default(pool_), 0);

  PoolPrivate priv = {
      .dpdk_priv = {.mbuf_data_room_size = XBUF_HEADROOM + XBUF_DATA,
                    .mbuf_priv_size = XBUF_RESERVE},
      .owner = this};

  rte_pktmbuf_pool_init(pool_, &priv.dpdk_priv);
  rte_mempool_obj_iter(pool_, InitPacket, nullptr);
  CHECK_EQ(pool_->populated_size, capacity);
}

/*
void PacketPool::CreatePools(size_t capacity) {
  InitDpdk();

  rte_dump_physmem_layout(stdout);

  int sid = CONFIG.nic.socket;
  LOG(INFO) << "Creating DpdkPacketPool for " << capacity << " packets on node "
            << sid;
  pools_[sid] = std::make_shared<DpdkPacketPool>(capacity, sid);

  CHECK(pools_[sid]) << "Packet pool allocation on node " << sid << " failed!";
}

PacketPool::PacketPool(size_t capacity, int socket_id) {
  static int next_id_;
  name_ = "PacketPool" + std::to_string(next_id_++);

  LOG(INFO) << name_ << " requests for " << capacity << " packets";

  pool_ = rte_mempool_create_empty(name_.c_str(), capacity, sizeof(Packet),
                                   capacity > 1024 ? kMaxCacheSize : 0,
                                   sizeof(PoolPrivate), socket_id, 0);
  CHECK_NOTNULL(pool_);

  CHECK_EQ(rte_mempool_set_ops_byname(pool_, "ring_mp_mc", NULL), 0);
}
 */

PacketPool::~PacketPool() { rte_mempool_free(pool_); }

/*
void PacketPool::PostPopulate() {
  PoolPrivate priv = {
      .dpdk_priv = {.mbuf_data_room_size = XBUF_HEADROOM + XBUF_DATA,
                    .mbuf_priv_size = XBUF_RESERVE},
      .owner = this};

  rte_pktmbuf_pool_init(pool_, &priv.dpdk_priv);
  rte_mempool_obj_iter(pool_, InitPacket, nullptr);
  CHECK_NE(capacity(), 0);

  LOG(INFO) << name_ << " has been created with " << capacity() << " packets";
}
 */

/*
DpdkPacketPool::DpdkPacketPool(size_t capacity, int socket_id)
    : PacketPool(capacity, socket_id) {
  CHECK_GE(rte_mempool_populate_default(pool_), 0);
  PostPopulate();
}
 */

} // namespace xlb
