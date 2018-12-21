#include "utils/rcu.h"
#include "range.h"

namespace xlb {
namespace utils {

    template <typename T>
    using RCU = RCU<T>;


template <typename T> __thread int RCU<T>::tid_;

    template <typename... Args>
    RCU<T>::RCU(int num, Args &&... args) {

    }

template <typename T> RCU<T>::RCU(int num) {
    for (auto i : range(0, num)) {
        data_[i] =
    }

}

} // namespace utils
} // namespace xlb
