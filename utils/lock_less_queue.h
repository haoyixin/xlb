#pragma once

#include <rte_malloc.h>
#include <rte_memory.h>

#include <glog/logging.h>

#include "3rdparty/llring.h"

#include "utils/common.h"

namespace xlb::utils {

// A wrapper class for llring.
template <typename T>
class LockLessQueue {
  static_assert(std::is_pointer<T>::value,
                "LockLessQueue only supports pointer types");

 public:
  enum Error : int8_t {
    NONE = 0,
    NOBUF = -LLRING_ERR_NOBUF,
    QUOT = -LLRING_ERR_QUOT,
    NOENT = -LLRING_ERR_NOENT
  };

  static const size_t kDefaultRingSize = 256;

  explicit LockLessQueue(int socket = SOCKET_ID_ANY,
                         size_t capacity = kDefaultRingSize,
                         bool single_producer = true,
                         bool single_consumer = true)
      : capacity_(align_ceil_pow2(capacity)), socket_(socket) {
    ring_ = reinterpret_cast<struct llring *>(rte_malloc_socket(
        nullptr, llring_bytes_with_slots(capacity_), alignof(llring), socket_));
    CHECK_NOTNULL(ring_);
    CHECK_EQ(llring_init(ring_, capacity_, single_producer, single_consumer),
             0);
  }

  ~LockLessQueue() {
    if (ring_) rte_free(ring_);
  }

  // error codes: -1 is Quota exceeded. The objects have been enqueued,
  // but the high water mark is exceeded. -2 is not enough room in the
  // ring to enqueue; no object is enqueued.
  Error Push(T obj) {
    return llring_enqueue(ring_, reinterpret_cast<void *>(obj));
  }

  Error Push(T *objs, size_t count) {
    return static_cast<Error>(
        llring_enqueue_bulk(ring_, reinterpret_cast<void **>(objs), count));
  }

  Error Pop(T &obj) {
    return llring_dequeue(ring_, reinterpret_cast<void **>(&obj));
  }

  size_t Pop(T *objs, size_t count) {
    return llring_dequeue_burst(ring_, reinterpret_cast<void **>(objs), count);
  }

  // capacity will be one less than specified
  size_t Capacity() { return capacity_; }

  size_t Size() { return llring_count(ring_); }

  bool Empty() { return llring_empty(ring_); }

  bool Full() { return llring_full(ring_); }

  int Resize(size_t new_capacity) {
    if (new_capacity <= Size() || (new_capacity & (new_capacity - 1))) {
      return -1;
    }

    auto *new_ring = reinterpret_cast<struct llring *>(rte_malloc_socket(
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
  struct llring *ring_;  // class's ring buffer
  size_t capacity_;      // the size of the backing ring buffer
};

}  // namespace xlb::utils
