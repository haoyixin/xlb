#ifndef XLB_PACKET_POOL_H
#define XLB_PACKET_POOL_H

#include <memory>

#include "config.h"
#include "packet.h"

namespace xlb {

// PacketPool is a C++ wrapper for DPDK rte_mempool. It has a pool of
// pre-populated Packet objects, which can be fetched via Alloc().
// Alloc() and Free() are thread-safe.
class PacketPool {
public:
  //  static PacketPool *GetPool(int node) { return pools_[node].get(); }

  //  static void CreatePools(size_t capacity = CONFIG.packet_pool);

  PacketPool(size_t capacity = CONFIG.mem.packet_pool,
             int socket_id = CONFIG.nic.socket);
  ~PacketPool();

  // PacketPool is neither copyable nor movable.
  PacketPool(const PacketPool &) = delete;
  PacketPool &operator=(const PacketPool &) = delete;

  // Allocate a packet from the pool, with specified initial packet size.
  Packet *Alloc(size_t len = 0) {
    Packet *pkt = reinterpret_cast<Packet *>(rte_pktmbuf_alloc(pool_));
    if (pkt) {
      pkt->pkt_len_ = len;
      pkt->data_len_ = len;
      // TODO: check
    }
    return pkt;
  }

  // TODO: implement it
  /*
  bool AllocBulk(Packet **pkts, size_t count, size_t len = 0);
   */

  // The number of total packets in the pool. 0 if initialization failed.
  size_t capacity() const { return pool_->populated_size; }

  // The number of available packets in the pool. Approximate by nature.
  size_t size() const { return rte_mempool_avail_count(pool_); }

  // TODO: not to expose this
  rte_mempool *pool() { return pool_; }

protected:
  static const size_t kDefaultCapacity = (1 << 16) - 1; // 64k - 1
  static const size_t kMaxCacheSize = 512;              // per-core cache size

  // Subclasses are expected to call this function in their constructor
  //  void PostPopulate();

  //  std::string name_;
  rte_mempool *pool_;

private:
  // Per-node packet pools
  // TODO: singleton
  //  static std::shared_ptr<PacketPool> pools_[RTE_MAX_NUMA_NODES];

  friend class Packet;
};

} // namespace xlb

#endif // XLB_PACKET_POOL_H
