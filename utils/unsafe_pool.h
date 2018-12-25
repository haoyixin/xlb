#ifndef XLB_UTILS_FIXED_POOL_H
#define XLB_UTILS_FIXED_POOL_H

#include "utils/allocator.h"
#include "utils/range.h"

#include "functional"
#include "stack"
#include "vector"

namespace xlb {
namespace utils {

// Simple pool without check for performance
template <typename T> class UnsafePool {
  static_assert(std::is_pod<T>::value);

public:
  // simple guard pointer for raii
  class guard_ptr {
  public:
    explicit guard_ptr(UnsafePool *pool, T *obj) : pool_(pool), obj_(obj) {}
    ~guard_ptr() { pool_->Put(obj_); }

    T *operator->() { return obj_; }

  private:
    UnsafePool *pool_;
    T *obj_;
  };

  explicit UnsafePool(size_t size, int socket = SOCKET_ID_ANY,
                std::function<void(size_t, T *)> init_func = default_init_func)
      : allocator_(socket), entries_(size, &allocator_), free_entry_indices_() {
    T *obj;
    for (auto i : range(1, size + 1)) {
      obj = &entries_[size - i];
      init_func(size - i, obj);
      free_entry_indices_.push(obj);
    }
  }

  // Get from a empty pool is UB
  T *Get() {
    T *obj = free_entry_indices_.top();
    free_entry_indices_.pop();
    return obj;
  }

  guard_ptr GetGuard() {
    T *obj = free_entry_indices_.top();
    free_entry_indices_.pop();
    return guard_ptr(this, obj);
  }

  // Put a pointer outside entries into pool is UB
  void Put(T *obj) { free_entry_indices_.push(obj); }

  bool Empty() { return free_entry_indices_.empty(); }

private:
  static void default_init_func(size_t, T *) {}

  MemoryResource allocator_;
  std::vector<T, std::experimental::pmr::polymorphic_allocator<T>> entries_;
  std::stack<T *> free_entry_indices_;
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_FIXED_POOL_H
