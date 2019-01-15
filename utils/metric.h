#ifndef XLB_UTILS_COUNTER_H
#define XLB_UTILS_COUNTER_H

#include <3rdparty/typestring.hh>
#include <bvar/bvar.h>
#include <glog/logging.h>

#include "utils/format.h"

namespace xlb {
namespace utils {

#define TSTR typestring_is

using _Adder = bvar::Adder<uint64_t>;
using _PerSecond = bvar::PerSecond<_Adder>;

template <typename P, typename C> struct Metric {
public:
  template <typename S, typename... SS> static void Expose() {
    CHECK_EQ(
        (internal<S>::count.expose_as(
            P::data(), utils::Format("%s_%s_count", C::data(), S::data()))),
        0);

    CHECK_EQ(
        (internal<S>::per_second.expose_as(
            P::data(), utils::Format("%s_%s_second", C::data(), S::data()))),
        0);

    if constexpr (sizeof...(SS) > 0)
      Expose<SS...>();
  }

  template <typename S> static auto &Adder() { return internal<S>::count; }

  template <typename S> static auto &PerSecond() {
    return internal<S>::per_second;
  }

private:
  template <typename S> struct internal {
    static _Adder count;
    static _PerSecond per_second;
  };
};

template <typename P, typename C>
template <typename S>
_Adder Metric<P, C>::internal<S>::count = {};

template <typename P, typename C>
template <typename S>
_PerSecond Metric<P, C>::internal<S>::per_second = {
    &Metric<P, C>::internal<S>::count};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_COUNTER_H
