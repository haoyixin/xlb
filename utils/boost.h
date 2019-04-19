#pragma once

#include "utils/common.h"

namespace xlb::utils {

using boost::algorithm::any_of;
using boost::algorithm::any_of_equal;

using boost::irange;
using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::algorithm::trim;

using boost::sort;
using boost::unique;

using boost::find_if;
using boost::intrusive_ptr;
using boost::remove_erase_if;

using boost::intrusive_ptr;

template <typename T>
using unsafe_intrusive_ref_counter =
    boost::intrusive_ref_counter<T, boost::thread_unsafe_counter>;

template <typename T>
using intrusive_ref_counter =
    boost::intrusive_ref_counter<T, boost::thread_safe_counter>;

}  // namespace xlb::utils
