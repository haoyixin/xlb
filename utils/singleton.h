#ifndef XLB_UTILS_SINGLETON_H
#define XLB_UTILS_SINGLETON_H

#include <utility>

namespace xlb {
namespace utils {

template <typename T> class Singleton {
public:
  template <typename... Args> static T &Get(Args &&... args) {
    static T value(std::forward<Args>(args)...);
    return value;
  }

  //  static void Reset() { Singleton<T>::Get().~T(); }
private:
  Singleton();
  ~Singleton();
};

template <typename T> class UnsafeSingleton {
public:
  template <typename... Args> static T *Set(Args &&... args) {
    value = new T(std::forward<Args>(args)...);
    return value;
  }

  static T *Get() { return value; }

private:
  static T *value;
  UnsafeSingleton();
  ~UnsafeSingleton();
};

    template <typename T> T *UnsafeSingleton<T>::value;

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_SINGLETON_H
