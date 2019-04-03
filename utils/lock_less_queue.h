#pragma once

#include <atomic>

#include <rte_ring.h>

#include <glog/logging.h>

#include "utils/allocator.h"
#include "utils/singleton.h"

namespace xlb::utils {

struct RingId {};

// A wrapper class for rte_ring.
template <typename T, bool SP, bool SC>
class LockLessQueue {
  static_assert(sizeof(T) <= 8);
  // TODO: union wrapper for unaligned type
  static_assert(alignof(T) == 8);

 public:
  static const size_t kDefaultRingSize = 512;

  explicit LockLessQueue(size_t capacity = kDefaultRingSize) {
    unsigned flags = 0;
    size_t actual_capacity = align_ceil_pow2(capacity);
    int socket = DefaultAllocator().socket();

    if constexpr (SP)
      flags |= RING_F_SP_ENQ;
    else if constexpr (SC)
      flags |= RING_F_SC_DEQ;

    auto name =
        Format("LockLessQueue[%d]",
               Singleton<std::atomic<size_t>, RingId>::instance().fetch_add(1));

    ring_ = rte_ring_create(name.c_str(), actual_capacity, socket, flags);

    DLOG(INFO) << "Created ring on socket: " << socket << " name: " << name
               << " capacity: " << actual_capacity
               << " error: " << rte_strerror(rte_errno);

    CHECK_NOTNULL(ring_);
  }

  ~LockLessQueue() {
    if (ring_) rte_ring_free(ring_);
  }

  bool Push(const T &obj) {
    if constexpr (SP)
      return rte_ring_sp_enqueue(ring_, reinterpret_cast<void *>(obj)) == 0;
    else
      return rte_ring_mp_enqueue(ring_, reinterpret_cast<void *>(obj)) == 0;
  }

  bool Push(T *objs, size_t count) {
    if constexpr (SP)
      return rte_ring_sp_enqueue_bulk(ring_, reinterpret_cast<void **>(objs),
                                      count, nullptr) == count;
    else
      return rte_ring_mp_enqueue_bulk(ring_, reinterpret_cast<void **>(objs),
                                      count, nullptr) == count;
  }

  bool Pop(T &obj) {
    if constexpr (SC)
      return rte_ring_sc_dequeue(ring_, reinterpret_cast<void **>(&obj)) == 0;
    else
      return rte_ring_mc_dequeue(ring_, reinterpret_cast<void **>(&obj)) == 0;
  }

  size_t Pop(T *objs, size_t count) {
    if constexpr (SC)
      return rte_ring_sc_dequeue_burst(ring_, reinterpret_cast<void **>(objs),
                                       count, nullptr);
    else
      return rte_ring_mc_dequeue_burst(ring_, reinterpret_cast<void **>(objs),
                                       count, nullptr);
  }

  size_t Capacity() { return rte_ring_get_capacity(ring_); }
  size_t Count() { return rte_ring_count(ring_); }
  size_t FreeCount() { return rte_ring_free_count(ring_); }

  bool Empty() { return rte_ring_empty(ring_) == 1; }
  bool Full() { return rte_ring_full(ring_) == 1; }

 private:
  struct rte_ring *ring_;  // class's ring buffer
};

}  // namespace xlb::utils
