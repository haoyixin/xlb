#include "packet_pool.h"

#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_mempool.h>

#include "dpdk.h"
#include "opts.h"

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

/*
void DoMunmap(rte_mempool_memhdr *memhdr, void *) {
  if (munmap(memhdr->addr, memhdr->len) < 0) {
    PLOG(WARNING) << "munmap()";
  }
}
 */

} // namespace

PacketPool *PacketPool::pools_[RTE_MAX_NUMA_NODES];

void PacketPool::CreatePools(size_t capacity) {
  InitDpdk(FLAGS_m);

  rte_dump_physmem_layout(stdout);

  for (int sid = 0; sid < utils::NumNumaNodes(); sid++) {
    LOG(INFO) << "Creating DpdkPacketPool for " << capacity
              << " packets on node " << sid;
    pools_[sid] = new DpdkPacketPool(capacity, sid);

    CHECK(pools_[sid])
        << "Packet pool allocation on node " << sid << " failed!";
  }
}

PacketPool::PacketPool(size_t capacity, int socket_id) {
  /*
  if (!IsDpdkInitialized()) {
    InitDpdk(FLAGS_m);
  }
   */

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

PacketPool::~PacketPool() {
  // munmap is triggered by the registered callback DoMunmap()
  rte_mempool_free(pool_);
}

/*
bool PacketPool::AllocBulk(Packet **pkts, size_t count, size_t len) {
  if (rte_mempool_get_bulk(pool_, reinterpret_cast<void **>(pkts), count) < 0) {
    return false;
  }

  // We must make sure that the following 12 fields are initialized
  // as done in rte_pktmbuf_reset(). We group them into two 16-byte stores.
  //
  // - 1st store: mbuf.rearm_data
  //   2B data_off == RTE_PKTMBUF_HEADROOM (XBUF_HEADROOM)
  //   2B refcnt == 1
  //   2B nb_segs == 1
  //   2B port == 0xff (0xffff should make more sense)
  //   8B ol_flags == 0
  //
  // - 2nd store: mbuf.rx_descriptor_fields1
  //   4B packet_type == 0
  //   4B pkt_len == len
  //   2B data_len == len
  //   2B vlan_tci == 0
  //   4B (rss == 0)       (not initialized by rte_pktmbuf_reset)
  //
  // We can ignore these fields:
  //   vlan_tci_outer == 0 (not required if ol_flags == 0)
  //   tx_offload == 0     (not required if ol_flags == 0)
  //   next == nullptr     (all packets in a mempool must already be nullptr)

  __m128i rearm = _mm_setr_epi16(XBUF_HEADROOM, 1, 1, 0xff, 0, 0, 0, 0);
  __m128i rxdesc = _mm_setr_epi32(0, len, len, 0);

  size_t i;

  // 4 at a time didn't help
  for (i = 0; i < (count & (~0x1)); i += 2) {
    // since the data is likely to be in the store buffer
    // as 64-bit writes, 128-bit read will cause stalls
    Packet *pkt0 = pkts[i];
    Packet *pkt1 = pkts[i + 1];

    _mm_store_si128(&pkt0->rearm_data_, rearm);
    _mm_store_si128(&pkt0->rx_descriptor_fields1_, rxdesc);
    _mm_store_si128(&pkt1->rearm_data_, rearm);
    _mm_store_si128(&pkt1->rx_descriptor_fields1_, rxdesc);
  }

  if (count & 0x1) {
    Packet *pkt = pkts[i];

    _mm_store_si128(&pkt->rearm_data_, rearm);
    _mm_store_si128(&pkt->rx_descriptor_fields1_, rxdesc);
  }

  // TODO: sanity check for packets
  return true;
}
 */

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
