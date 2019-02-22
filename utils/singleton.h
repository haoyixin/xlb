#pragma once

#include <utility>

namespace xlb::utils {

struct DefaultTag {};

template <typename T, typename Tag = DefaultTag>
class Singleton {
 public:
  template <typename... Args>
  static T &instance(Args &&... args) {
    static T instance_(std::forward<Args>(args)...);
    return instance_;
  }

  // private:
  Singleton() = delete;
  ~Singleton() = delete;
};

template <typename T, typename Tag = DefaultTag>
class UnsafeSingleton {
 public:
  UnsafeSingleton() = delete;
  ~UnsafeSingleton() { delete instance_; };

  template <typename... Args>
  static T *Init(Args &&... args) {
    instance_ = new T(std::forward<Args>(args)...);
    return instance_;
  }

  static T *instance() { return instance_; }

 private:
  static T *instance_;
};

template <typename T, typename Tag>
T *UnsafeSingleton<T, Tag>::instance_;

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
