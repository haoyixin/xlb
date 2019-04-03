#pragma once

#include <functional>
#include <stack>
#include <vector>

#include "glog/logging.h"

#include "utils/allocator.h"
#include "utils/boost.h"

namespace pmr = std::experimental::pmr;

namespace xlb::utils {

// Simple pool without check for performance
template <typename T>
class UnsafeIndexPool {
  static_assert(std::is_default_constructible<T>::value);

 public:
  explicit UnsafeIndexPool(
      idx_t size, std::function<void(idx_t, T &)> init_func = default_init_func)
      : entries_(size, &DefaultAllocator()), free_indexes_() {
    CHECK_GT(size, 0);
    for (auto idx : irange(0u, size)) {
      init_func(idx, entries_[idx]);
      free_indexes_.push(idx);
    }
  }

  T &operator[](idx_t idx) { return entries_[idx]; }

  // Get from a empty pool is UB
  idx_t Get() {
    auto idx = free_indexes_.top();
    free_indexes_.pop();
    return idx;
  }

  // Put a index outside entries or duplicate into pool is UB
  void Put(idx_t idx) {
    entries_[idx].~T();
    free_indexes_.push(idx);
  }

  // parameter entry is pointer of entry in entries, otherwise this is UB
  idx_t IndexOf(T *entry) { return entry - &entries_[0]; }

  bool Empty() { return free_indexes_.empty(); }

 private:
  static void default_init_func(idx_t, T &) {}

  std::vector<T, pmr::polymorphic_allocator<T>> entries_;
  std::stack<idx_t> free_indexes_;
};

}  // namespace xlb::utils
