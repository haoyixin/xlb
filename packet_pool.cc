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
void init_packet(rte_mempool *mp, void *, void *mbuf, unsigned index) {
  rte_pktmbuf_init(mp, nullptr, mbuf, index);

  auto *pkt = static_cast<Packet *>(mbuf);
  pkt->set_vaddr(pkt);
  pkt->set_paddr(rte_mempool_virt2iova(pkt));
}

}  // namespace

PacketPool::PacketPool(size_t capacity, int socket_id) {
  // TODO: support multi numa node

  LOG(INFO) << "[PacketPool] Creating with capacity: " << capacity
            << " packets on node: " << (socket_id == -1 ? 0 : socket_id);

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
  rte_mempool_obj_iter(pool_, init_packet, nullptr);
  CHECK_EQ(pool_->populated_size, capacity);
}

Packet *PacketPool::Alloc(size_t len) {
  auto *pkt = reinterpret_cast<Packet *>(rte_pktmbuf_alloc(pool_));
  if (pkt) {
    pkt->pkt_len_ = len;
    pkt->data_len_ = len;
    // TODO: check
  }
  return pkt;
}

PacketPool::~PacketPool() { rte_mempool_free(pool_); }

}  // namespace xlb
