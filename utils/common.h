/* This header file contains general (not XLB specific) C/C++ definitions */

#pragma once

#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <cxxabi.h>

#if __cplusplus < 201703L  // pre-C++17?
#error Must be built with C++17
#endif

/* Hint for performance optimization. Same as _nDCHECK() of TI compilers */
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
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// Used to explicitly mark the return value of a function as unused. If you are
// really sure you don't want to do anything with the return value of a function
// that has been marked WARN_UNUSED_RESULT, wrap it with this. Example:
//
//   std::unique_ptr<MyType> my_var = ...;
//   if (TakeOwnership(my_var.get()) == SUCCESS)
//     ignore_result(my_var.release());
//
template <typename T>
inline void ignore_result(const T &) {}

// An RAII holder for file descriptors.  When initialized with a file desciptor,
// takes
// ownership of the fd, meaning that when this holder instance is destroyed the
// fd is
// closed.  Primarily useful in unit tests where we want to ensure that previous
// tests
// have been cleaned up before starting new ones.
class unique_fd {
 public:
  // Constructs a unique fd that owns the given fd.
  unique_fd(int fd) : fd_(fd) {}

  // Move constructor
  unique_fd(unique_fd &&other) : fd_(other.fd_) { other.fd_ = -1; }

  // Destructor, which closes the fd if not -1.
  ~unique_fd() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  // Resets this instance and closes the fd held, if any.
  void reset() {
    if (fd_ != -1) {
      close(fd_);
    }
    fd_ = -1;
  }

  // Releases the held fd from ownership.  Returns -1 if no fd is held.
  int release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

  int get() const { return fd_; }

 private:
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(unique_fd);
};

// Inserts the given item into the container in sorted order.  Starts from the
// end of the container and swaps forward.  Assumes the container is already
// sorted.
template <typename T, typename U>
inline void InsertSorted(T &container, U &item) {
  container.push_back(item);

  for (size_t i = container.size() - 1; i > 0; --i) {
    auto &prev = container[i - 1];
    auto &cur = container[i];
    if (cur < prev) {
      std::swap(prev, cur);
    } else {
      break;
    }
  }
}

// Returns the absolute difference between `lhs` and `rhs`.
template <typename T>
T absdiff(const T &lhs, const T &rhs) {
  return lhs > rhs ? lhs - rhs : rhs - lhs;
}

struct PairHasher {
  template <typename T1, typename T2>
  std::size_t operator()(const std::pair<T1, T2> &p) const noexcept {
    // Adopted from Google's cityhash Hash128to64(), MIT licensed
    const uint64_t kMul = 0x9ddfea08eb382d69ULL;
    std::size_t x = std::hash<T1>{}(p.first);
    std::size_t y = std::hash<T2>{}(p.second);
    uint64_t a = (x ^ y) * kMul;
    a ^= (a >> 47);
    uint64_t b = (x ^ y) * kMul;
    b ^= (b >> 47);
    b *= kMul;
    return static_cast<size_t>(b);
  }
};

inline size_t hash_combine(size_t h1, size_t h2) {
  return h2 ^ (h1 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2));
}

template <typename T>
using unsafe_ptr = std::__shared_ptr<T, __gnu_cxx::_S_single>;

template <typename T, typename A, typename... Args>
inline unsafe_ptr<T> allocate_unsafe(const A &a, Args &&... args) {
  return std::__allocate_shared<T, __gnu_cxx::_S_single, A>(
      a, std::forward<Args>(args)...);
}

using idx_t = uint32_t;

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


