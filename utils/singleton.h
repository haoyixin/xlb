#ifndef XLB_UTILS_SINGLETON_H
#define XLB_UTILS_SINGLETON_H

#include <utility>

namespace xlb {
namespace utils {

struct DefaultTag {};

template <typename T, typename Tag = DefaultTag> class Singleton {
public:
  template <typename... Args> static T &instance(Args &&... args) {
    static T instance_(std::forward<Args>(args)...);
    return instance_;
  }

  //  static void Reset() { Singleton<T>::Get().~T(); }
private:
  Singleton();
  ~Singleton();
};

template <typename T, typename Tag = DefaultTag> class UnsafeSingleton {
public:
  template <typename... Args> static T *Init(Args &&... args) {
    instance_ = new T(std::forward<Args>(args)...);
    return instance_;
  }

  static T *instance() { return instance_; }

private:
  static T *instance_;
  UnsafeSingleton();
  ~UnsafeSingleton();
};

template <typename T, typename Tag>
T *UnsafeSingleton<T, Tag>::instance_;

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_SINGLETON_H
