#pragma once

#include <array>
#include <atomic>
#include <utility>

#include <glog/logging.h>

namespace xlb::utils {

struct DefaultTag {};

template <typename T, typename Tag = DefaultTag>
class Singleton {
 public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T &instance() {
    static T instance_{};
    return instance_;
  }
};

template <typename T, typename Tag = DefaultTag>
class UnsafeSingleton {
 public:
  UnsafeSingleton() = delete;
  ~UnsafeSingleton() = delete;

  template <typename... Args>
  static T &Init(Args &&... args) {
    return *(new (&instance_.value) T(std::forward<Args>(args)...));
  }

  static T &instance() { return instance_.value; }

 private:
  union Wrapper {
    constexpr Wrapper() : raw{0} {}
    ~Wrapper() { value.~T(); }

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
    if (!inited_.load(std::memory_order_acquire)) {
      new (instance_.data()) T(std::forward<Args>(args)...);
      inited_.store(true, std::memory_order_release);
    }
    return instance();
  }

  static T &instance() { return *reinterpret_cast<T *>(instance_.data()); }

 private:
  class Dtor {
    Dtor() = default;
    ~Dtor() {
      if (inited_.load(std::memory_order_acquire)) instance.~T();
    }
  };

  alignas(alignof(T)) static __thread std::array<uint8_t, sizeof(T)> instance_;

  static __thread std::atomic<bool> inited_;
  static thread_local Dtor dtor_;
};

template <typename T, typename Tag>
alignas(alignof(T)) __thread std::array<uint8_t, sizeof(T)> UnsafeSingletonTLS<
    T, Tag>::instance_;

template <typename T, typename Tag>
__thread std::atomic<bool> UnsafeSingletonTLS<T, Tag>::inited_ = false;

template <typename T, typename Tag>
thread_local
    typename UnsafeSingletonTLS<T, Tag>::Dtor UnsafeSingletonTLS<T, Tag>::dtor_;

}  // namespace xlb::utils
