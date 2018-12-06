#ifndef XLB_UTILS_ALLOCATOR_H
#define XLB_UTILS_ALLOCATOR_H

#include <cstddef>

#include <rte_config.h>
#include <rte_malloc.h>

#include <experimental/memory_resource>
#include <new>

namespace xlb {
namespace utils {

template <typename T> class Allocator {
public:
  typedef T value_type;

  Allocator() = default;

  template <typename U> constexpr Allocator(const Allocator<U> &) noexcept {}

  // Will be allocated on the socket which calling this.
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

class MemoryResource : public std::experimental::pmr::memory_resource {
public:
  explicit MemoryResource(int socket) : socket_(socket) {}

protected:
  void *do_allocate(std::size_t bytes, std::size_t) override {
    if (auto p =
            rte_malloc_socket(nullptr, bytes, RTE_CACHE_LINE_SIZE, socket_))
      return p;

    throw std::bad_alloc();
  }

  void do_deallocate(void *p, std::size_t, std::size_t) override {
    rte_free(p);
  }

  bool do_is_equal(const std::experimental::pmr::memory_resource &other) const
      noexcept override {
    return this == &other;
  }

private:
  int socket_;
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_ALLOCATOR_H
