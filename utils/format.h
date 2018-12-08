// C's sprintf-like functionality for std::string

#ifndef XLB_UTILS_FORMAT_H
#define XLB_UTILS_FORMAT_H

#include <cstdarg>
#include <string>

namespace xlb {
namespace utils {

std::string FormatVarg(const char *fmt, va_list ap);
[[gnu::format(printf, 1, 2)]] std::string Format(const char *fmt, ...);

int ParseVarg(const std::string &s, const char *fmt, va_list ap);
[[gnu::format(scanf, 2, 3)]] int Parse(const std::string &s, const char *fmt,
                                       ...);

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_FORMAT_H
