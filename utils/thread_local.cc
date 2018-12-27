#include "utils/thread_local.h"
#include "range.h"

namespace xlb {
namespace utils {

template <typename T, typename R> __thread size_t ThreadLocal<T, R>::tid_ = {};

} // namespace utils
} // namespace xlb
