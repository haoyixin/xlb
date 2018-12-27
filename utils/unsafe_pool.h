#ifndef XLB_UTILS_FIXED_POOL_H
#define XLB_UTILS_FIXED_POOL_H

#include "utils/allocator.h"
#include "utils/range.h"

#include "glog/logging.h"

#include <functional>
#include <stack>
#include <vector>

namespace xlb {
namespace utils {

// Simple pool without check for performance
template <typename T> class UnsafePool {
  static_assert(std::is_pod<T>::value);

public:
  explicit UnsafePool(
      size_t size, int socket = SOCKET_ID_ANY,
      std::function<void(size_t, T *)> init_func = default_init_func)
      : allocator_(socket), entries_(size, &allocator_), free_entry_pointer_() {
    CHECK_GT(size, 0);
    for (auto i : range(size - 1, 0).step(-1)) {
      init_func(i, &entries_[i]);
      free_entry_pointer_.push(&entries_[i]);
    }
  }

  // Get from a empty pool is UB
  T *get() {
    T *obj = free_entry_pointer_.top();
    free_entry_pointer_.pop();
    return obj;
  }

  auto get_shared() {
    return std::shared_ptr<T>(get(), [this](T *p) { put(p); });
  }

  // Put a pointer outside entries into pool is UB
  void put(T *obj) { free_entry_pointer_.push(obj); }

  bool empty() { return free_entry_pointer_.empty(); }

private:
  static void default_init_func(size_t, T *) {}

  MemoryResource allocator_;
  std::vector<T, std::experimental::pmr::polymorphic_allocator<T>> entries_;
  std::stack<T *> free_entry_pointer_;
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_FIXED_POOL_H
