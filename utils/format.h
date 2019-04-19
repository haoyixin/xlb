// C's sprintf-like functionality for std::string

#pragma once

#include "utils/common.h"

namespace xlb::utils {

std::string FormatVarg(const char *fmt, va_list ap);
[[gnu::format(printf, 1, 2)]] std::string Format(const char *fmt, ...);

int ParseVarg(const std::string &s, const char *fmt, va_list ap);
[[gnu::format(scanf, 2, 3)]] int Parse(const std::string &s, const char *fmt,
                                       ...);

}  // namespace xlb::utils
