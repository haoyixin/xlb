#pragma once

#include <cstddef>

#include <rte_config.h>
#include <rte_malloc.h>

#include <glog/logging.h>

#include <experimental/memory_resource>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>

#include "utils/common.h"
#include "utils/singleton.h"

namespace pmr = std::experimental::pmr;

namespace xlb {

#define ALLOC &utils::UnsafeSingleton<utils::MemoryResource>::instance()

}  // namespace xlb

namespace xlb::utils {

template <typename T>
using vector = std::vector<T, pmr::polymorphic_allocator<T>>;

template <typename K, typename V>
using unordered_map =
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>,
                       pmr::polymorphic_allocator<std::pair<const K, V>>>;

template <typename K, typename V>
using unordered_multimap =
    std::unordered_multimap<K, V, std::hash<K>, std::equal_to<K>,
                            pmr::polymorphic_allocator<std::pair<const K, V>>>;

template <typename T>
class Allocator {
 public:
  typedef T value_type;

  Allocator() = default;

  template <typename U>
  constexpr explicit Allocator(const Allocator<U> &) noexcept {}

  // Will be allocated on the socket which calling this.
  T *allocate(std::size_t n) {
    if (n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
    if (auto p =
            static_cast<T *>(rte_malloc(nullptr, n * sizeof(T), alignof(T))))
      return p;
    throw std::bad_alloc();
  }

  void deallocate(T *p, std::size_t n) noexcept { rte_free(p); }
};

// Stateful allocator support in std is like a shit, so we use pmr in C++ 17.
// Fortunately, the frequency of calling allocate/do_allocate is not very high
// in our project, so we can bear the overhead of a virtual function.
class MemoryResource : public std::experimental::pmr::memory_resource {
 public:
  explicit MemoryResource(int socket) : socket_(socket) {}

  int socket() const { return socket_; }

 protected:
  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (auto p = rte_malloc_socket(nullptr, bytes, alignment, socket_))
      return p;

    throw std::bad_alloc();
  }

  void do_deallocate(void *p, std::size_t, std::size_t) override {
    rte_free(p);
  }

  bool do_is_equal(const pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }

 private:
  int socket_;

  friend struct INew;
};

struct INew {
  static void *operator new(std::size_t sz) {
    return (ALLOC)->do_allocate(sz, 0);
  }
  static void *operator new(std::size_t sz, std::align_val_t al) {
    return (ALLOC)->do_allocate(sz, static_cast<size_t>(al));
  }
  static void *operator new[](std::size_t sz) {
    return (ALLOC)->do_allocate(sz, 0);
  }
  static void *operator new[](std::size_t sz, std::align_val_t al) {
    return (ALLOC)->do_allocate(sz, static_cast<size_t>(al));
  }
  static void operator delete(void *ptr) { (ALLOC)->do_deallocate(ptr, 0, 0); }
  static void operator delete[](void *ptr) {
    (ALLOC)->do_deallocate(ptr, 0, 0);
  }
};

inline void InitDefaultAllocator(int socket) {
  UnsafeSingleton<MemoryResource>::Init(socket);
}

template <typename T, typename... Args>
inline std::shared_ptr<T> make_shared(Args &&... args) {
  return std::allocate_shared<T, pmr::polymorphic_allocator<T>>(
      ALLOC, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline unsafe_ptr<T> make_unsafe(Args &&... args) {
  return allocate_unsafe<T, pmr::polymorphic_allocator<T>>(
      ALLOC, std::forward<Args>(args)...);
}

template <typename T>
inline utils::vector<T> make_vector(size_t n) {
  auto vec = utils::vector<T>(ALLOC);
  vec.reserve(n);
  return vec;
}

}  // namespace xlb::utils
