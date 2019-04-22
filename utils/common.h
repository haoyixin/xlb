#pragma once

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cxxabi.h>
#include <x86intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <experimental/filesystem>
#include <experimental/memory_resource>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/irange.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <3rdparty/bvar/combiner.h>
#include <3rdparty/typestring.hh>

#include <bvar/bvar.h>
#include <glog/logging.h>

#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_ring.h>

#if __cplusplus < 201703L  // pre-C++17?
#error Must be built with C++17
#endif

/* Hint for performance optimization */
#define promise(cond)                     \
  ({                                      \
    if (!(cond)) __builtin_unreachable(); \
  })
#define promise_unreachable() __builtin_unreachable();

#ifndef likely
#define likely(x) __builtin_expect((x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((x), 0)
#endif

#define member_type(type, member) typeof(((type *)0)->member)

#define ARR_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

static inline uint64_t align_floor(uint64_t v, uint64_t align) {
  return v - (v % align);
}

static inline uint64_t align_ceil(uint64_t v, uint64_t align) {
  return align_floor(v + align - 1, align);
}

static inline uint64_t align_ceil_pow2(uint64_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;

  return v + 1;
}

#define __cacheline_aligned __attribute__((aligned(64)))

/* For x86_64. DMA operations are not safe with these macros */
#define INST_BARRIER() asm volatile("" ::: "memory")
#define LOAD_BARRIER() INST_BARRIER()
#define STORE_BARRIER() INST_BARRIER()
#define FULL_BARRIER() asm volatile("mfence" ::: "memory")

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) TypeName(const TypeName &) = delete
// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) void operator=(const TypeName &) = delete
// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &) = delete;     \
  void operator=(const TypeName &) = delete
// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// Used to explicitly mark the return value of a function as unused.
template <typename T>
inline void ignore_result(const T &) {}

inline size_t hash_combine(size_t h1, size_t h2) {
  return h2 ^ (h1 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2));
}

// Thread unsafe version of shared_ptr
template <typename T>
using unsafe_ptr = std::__shared_ptr<T, __gnu_cxx::_S_single>;

template <typename T, typename A, typename... Args>
inline unsafe_ptr<T> allocate_unsafe(const A &a, Args &&... args) {
  return std::__allocate_shared<T, __gnu_cxx::_S_single, A>(
      a, std::forward<Args>(args)...);
}

template <typename T>
struct expose_protected_ctor : public T {
  template <typename... Args>
  explicit expose_protected_ctor(Args &&... args)
      : T(std::forward<Args>(args)...) {}
};

template <typename T>
std::string demangle() {
  char *c = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  std::string s{c};
  std::free(c);

  return s;
}

template <typename T>
std::string demangle(T *obj) {
  char *c = abi::__cxa_demangle(typeid(*obj).name(), 0, 0, 0);
  std::string s{c};
  std::free(c);

  return s;
}

namespace pmr = std::experimental::pmr;
namespace fs = std::experimental::filesystem;

#define F_LOG(severity) (LOG(severity) << "[" << __FUNCTION__ << "] ")

#if DCHECK_IS_ON()

#define F_DLOG(severity) (DLOG(severity) << "[" << __FUNCTION__ << "] ")
#define F_DVLOG(verboselevel) DVLOG(verboselevel) << "[" << __FUNCTION__ << "] "

#else

#define F_DLOG(severity) \
  true ? (void)0 : google::LogMessageVoidify() & LOG(severity)

#define F_DVLOG(verboselevel)         \
  (true || !VLOG_IS_ON(verboselevel)) \
      ? (void)0                       \
      : google::LogMessageVoidify() & LOG(INFO)

#endif
