#ifndef XLB_UTILS_LOCK_LESS_QUEUE_H
#define XLB_UTILS_LOCK_LESS_QUEUE_H

#include <rte_malloc.h>
#include <rte_memory.h>

#include <glog/logging.h>

#include "3rdparty/llring.h"

namespace xlb {
namespace utils {

// A wrapper class for llring that extends the abstract class Queue. Takes a
// template argument T which is the type to be enqueued and dequeued.
template <typename T> class LockLessQueue {
  static_assert(std::is_pointer<T>::value,
                "LockLessQueue only supports pointer types");

public:
  static const size_t kDefaultRingSize = 256;

  // Construct a new queue. Takes the size of backing ring buffer (must power of
  // two and entries available will be one less than specified. default is 256),
  // boolean where if true, queue is in single producer mode or if false, queue
  // is in multi producer mode, boolean where if true, queue is in single
  // consumer mode, or if false, queue is in multi consumer mode. default for
  // both booleans is true.
  LockLessQueue(int socket = SOCKET_ID_ANY, size_t capacity = kDefaultRingSize,
                bool single_producer = true, bool single_consumer = true)
      : capacity_(capacity), socket_(socket) {
    CHECK_EQ(capacity % 2, 0);
    ring_ = reinterpret_cast<struct llring *>(rte_malloc_socket(
        NULL, llring_bytes_with_slots(capacity_), alignof(llring), socket_));
    CHECK_NOTNULL(ring_);
    CHECK_EQ(llring_init(ring_, capacity_, single_producer, single_consumer),
             0);
  }

  virtual ~LockLessQueue() {
    if (ring_)
      rte_free(ring_);
  }

  // error codes: -1 is Quota exceeded. The objects have been enqueued,
  // but the high water mark is exceeded. -2 is not enough room in the
  // ring to enqueue; no object is enqueued.
  int Push(T obj) {
    return llring_enqueue(ring_, reinterpret_cast<void *>(obj));
  }

  int Push(T *objs, size_t count) {
    if (!llring_enqueue_bulk(ring_, reinterpret_cast<void **>(objs), count)) {
      return count;
    }
    return 0;
  }

  int Pop(T &obj) {
    return llring_dequeue(ring_, reinterpret_cast<void **>(&obj));
  }

  int Pop(T *objs, size_t count) {
    if (!llring_dequeue_bulk(ring_, reinterpret_cast<void **>(objs), count)) {
      return count;
    }
    return 0;
  }

  // capacity will be one less than specified
  size_t capacity() { return capacity_; }

  size_t size() { return llring_count(ring_); }

  bool empty() { return llring_empty(ring_); }

  bool full() { return llring_full(ring_); }

  int Resize(size_t new_capacity) {
    if (new_capacity <= size() || (new_capacity & (new_capacity - 1))) {
      return -1;
    }

    llring *new_ring = reinterpret_cast<struct llring *>(rte_malloc_socket(
        NULL, llring_bytes_with_slots(new_capacity), alignof(llring), socket_));
    CHECK(new_ring);

    if (llring_init(new_ring, new_capacity, ring_->common.sp_enqueue,
                    ring_->common.sc_dequeue) != 0) {
      rte_free(new_ring);
      return -1;
    }

    void *obj;
    while (llring_dequeue(ring_, reinterpret_cast<void **>(&obj)) == 0) {
      llring_enqueue(new_ring, obj);
    }

    rte_free(ring_);
    ring_ = new_ring;
    capacity_ = new_capacity;
    return 0;
  }

private:
  int socket_;
  struct llring *ring_; // class's ring buffer
  size_t capacity_;     // the size of the backing ring buffer
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_LOCK_LESS_QUEUE_H
