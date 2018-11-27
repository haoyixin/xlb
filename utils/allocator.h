//
// Created by haoyixin on 11/23/18.
//

#ifndef XLB_UTILS_ALLOCATOR_H
#define XLB_UTILS_ALLOCATOR_H

#include <cstddef>
#include <rte_config.h>
#include <rte_malloc.h>
#include <new>

namespace xlb {
namespace utils {

template <typename T> class Allocator {
public:
  typedef T value_type;

  Allocator() = default;

  template <typename U> constexpr Allocator(const Allocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    if (n > std::size_t(-1) / sizeof(T))
      throw std::bad_alloc();
    if (auto p = static_cast<T *>(
            rte_malloc(NULL, n * sizeof(T), RTE_CACHE_LINE_SIZE)))
      return p;
    throw std::bad_alloc();
  }

  void deallocate(T *p, std::size_t n) noexcept { rte_free(p); }
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_ALLOCATOR_H
