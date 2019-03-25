#pragma once

#include <array>
#include <utility>

namespace xlb::utils {

struct DefaultTag {};

template <typename T, typename Tag = DefaultTag>
class Singleton {
 public:
  static T &instance() {
    alignas(64) static T instance_{};
    return instance_;
  }

 private:
    alignas(64) static std::array<uint8_t, sizeof(T)> instance_;
  Singleton() = delete;
  ~Singleton() = delete;
};

template <typename T, typename Tag = DefaultTag>
class UnsafeSingleton {
 public:
  UnsafeSingleton() = delete;
  ~UnsafeSingleton() { reinterpret_cast<T *>(instance_.data())->~T(); };

  template <typename... Args>
  static T &Init(Args &&... args) {
    new (instance_.data()) T(std::forward<Args>(args)...);
    return *reinterpret_cast<T *>(instance_.data());
  }

  static T &instance() { return *reinterpret_cast<T *>(instance_.data()); }

 private:
  alignas(64) static std::array<uint8_t, sizeof(T)> instance_;
};

template <typename T, typename Tag>
alignas(64) std::array<uint8_t, sizeof(T)> UnsafeSingleton<T, Tag>::instance_;

/*
template <typename T, typename Tag = DefaultTag> class UnsafeSingletonTLS {
public:
  template <typename... Args> static T *InitInThread(Args &&... args) {
    instance_ = new T(std::forward<Args>(args)...);
    return instance_;
  }

  static void InitInThread(T *instance) { instance_ = instance; }

  static T *instance() { return instance_; }

private:
  static __thread T *instance_;
  UnsafeSingletonTLS();
  ~UnsafeSingletonTLS();
};

template <typename T, typename Tag>
__thread T *UnsafeSingletonTLS<T, Tag>::instance_;
 */

}  // namespace xlb::utils
