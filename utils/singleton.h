#pragma once

#include "utils/common.h"

namespace xlb::utils {

struct DefaultTag {};

template <typename T, typename Tag = DefaultTag>
class Singleton {
 public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T &instance() {
    static logger_t logger{};
    static T instance_{};
    return instance_;
  }

 private:
  struct logger_t {
    logger_t() {
      F_DLOG(INFO) << "creating type: " << demangle<T>()
                   << " tag: " << demangle<Tag>();
    }

    ~logger_t() {
      F_DLOG(INFO) << "destructing type: " << demangle<T>()
                   << " tag: " << demangle<Tag>();
    }
  };
};

template <typename T, typename Tag = DefaultTag>
class UnsafeSingleton {
 public:
  UnsafeSingleton() = delete;
  ~UnsafeSingleton() = delete;

  template <typename... Args>
  static T &Init(Args &&... args) {
    F_DLOG(INFO) << "[Unsafe] initializing type: " << demangle<T>()
                 << " tag: " << demangle<Tag>();
    return *(new (&instance_.value) T(std::forward<Args>(args)...));
  }

  static T &instance() { return instance_.value; }

 private:
  union Wrapper {
    constexpr Wrapper() : raw{0} {}
    ~Wrapper() {
      F_DLOG(INFO) << "[Unsafe] destructing type: " << demangle<T>()
                   << " tag: " << demangle<Tag>();
      value.~T();
    }

    T value;
    std::array<uint8_t, sizeof(T)> raw;
  };

  static Wrapper instance_;
};

template <typename T, typename Tag>
typename UnsafeSingleton<T, Tag>::Wrapper UnsafeSingleton<T, Tag>::instance_;

template <typename T, typename Tag = DefaultTag>
class UnsafeSingletonTLS {
 public:
  UnsafeSingletonTLS() = delete;
  ~UnsafeSingletonTLS() = delete;

  template <typename... Args>
  static T &Init(Args &&... args) {
    F_DLOG(INFO) << "[TLS] initializing type: " << demangle<T>()
                 << " tag: " << demangle<Tag>();
    if (!dtor_.inited) {
      new (instance_.data()) T(std::forward<Args>(args)...);
      dtor_.inited = true;
    }
    return instance();
  }

  static T &instance() { return *reinterpret_cast<T *>(instance_.data()); }

 private:
  struct dtor_t {
    dtor_t() : inited(false){};
    ~dtor_t() {
      F_DLOG(INFO) << "[TLS] destructing type: " << demangle<T>()
                   << " tag: " << demangle<Tag>();
      if (inited) instance().~T();
    }

    bool inited;
  };

  static thread_local dtor_t dtor_;

  alignas(alignof(T)) static __thread std::array<uint8_t, sizeof(T)> instance_;
};

template <typename T, typename Tag>
alignas(alignof(T)) __thread std::array<uint8_t, sizeof(T)> UnsafeSingletonTLS<
    T, Tag>::instance_;

template <typename T, typename Tag>
thread_local typename UnsafeSingletonTLS<T, Tag>::dtor_t
    UnsafeSingletonTLS<T, Tag>::dtor_;

}  // namespace xlb::utils
