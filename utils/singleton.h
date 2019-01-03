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

  static void Reset() { Singleton<T>::Get().~T(); }

private:
  Singleton();
  ~Singleton();
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_SINGLETON_H
