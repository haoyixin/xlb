#pragma once

#include "runtime/common.h"
#include "runtime/config.h"
#include "runtime/packet.h"

namespace xlb {

// PacketPool is a C++ wrapper for DPDK rte_mempool. It has a pool of
// pre-populated Packet objects, which can be fetched via Alloc().
// Alloc() and Free() are thread-safe.
class PacketPool {
 public:
  explicit PacketPool(size_t capacity = CONFIG.mem.packet_pool,
                      int socket_id = CONFIG.nic.socket);
  ~PacketPool();

  // PacketPool is neither copyable nor movable.
  PacketPool(const PacketPool &) = delete;
  PacketPool &operator=(const PacketPool &) = delete;

  // Allocate a packet from the pool, with specified initial packet size.
  inline Packet *Alloc(size_t len = 0);

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
  static const size_t kDefaultCapacity = (1 << 16) - 1;  // 64k - 1
  static const size_t kMaxCacheSize = 512;               // per-core cache size

 private:
  rte_mempool *pool_;
  friend class Packet;
};

}  // namespace xlb
