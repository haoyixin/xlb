#ifndef XLB_UTILS_RCU_H
#define XLB_UTILS_RCU_H

#include "utils/lock_less_queue.h"
#include "utils/range.h"
#include <functional>
#include <rte_lru.h>

namespace xlb {
namespace utils {

template <typename T> class alignas(64) RCU {

public:
  using OpFunc = std::function<void(T *)>;

  // This must be big enough, we are not currently dealing with ops_ full
  static const size_t kDefaultOpsLength = 4096;
  static const size_t kMaxThread = 32;

  template <typename... Args>
  explicit RCU(int num_threads, int socket = SOCKET_ID_ANY, Args &&... args)
      : socket_(socket), data_pointer_(), data_(), ops_pointer_(), ops_(),
        registered_num_() {
    CHECK_LE(num_threads, kMaxThread);

    for (auto i : range(0, num_threads)) {
      new (data_ + i) T(std::forward<Args>(args)...);
      new (ops_ + i)
          LockLessQueue<OpFunc>(socket_, kDefaultOpsLength, false, true);
    }
    // 0 is reserved
    Register(0);
  }

  // Best case is core id equal to tid
  void Register(int tid) {
    // If our thread didn't call this, tid will be zero (for default usage)
    CHECK_GE(tid, 0);
    tid_ = tid;

    auto offset = registered_num_.fetch_add(1);
    data_pointer_[tid] = data_ + offset;
    ops_pointer_[tid] = ops_ + offset;
  }

  // This will hardly fail, but there may be places that are not well thought
  // out.
  void Sync() {
    for (;;) {
      OpFunc func;
      if (ops_pointer_[tid_]->Pop(func) != 0)
        break;

      func(data_[tid_]);
    }
  }

  void Update(OpFunc func) {
    for (auto &ops : ops_)
      ops.Push(func);
  }

  size_t NumThreads() { return registered_num_.load(); }

  static int Tid() { return tid_; }

  // This is use by someone who care about update result
  template <typename V, typename... Args> auto Map(Args &&... args) {
    auto ret = CuckooMap<int, V>(socket_);

    for (size_t i = 0; i < kMaxThread; i++)
      if (data_pointer_[i])
        ret.Emplace(i, std::forward<Args>(args)...);

    return ret;
  }

private:
  int socket_;

  alignas(64) T *data_pointer_[kMaxThread];
  T data_[kMaxThread];

  alignas(64) LockLessQueue<OpFunc> *ops_pointer_[kMaxThread];
  LockLessQueue<OpFunc> ops_[kMaxThread];

  std::atomic<size_t> registered_num_;

  static __thread int tid_;
};

template <typename T> __thread int RCU<T>::tid_;

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_RCU_H
