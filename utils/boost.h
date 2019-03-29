#pragma once

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/range/irange.hpp>
//#include <boost/range/begin.hpp>
//#include <boost/range/end.hpp>
//#include <boost/functional/hash.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

namespace xlb::utils {

using boost::algorithm::any_of;
using boost::algorithm::any_of_equal;

using boost::irange;
using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::algorithm::trim;

// using boost::begin;
// using boost::end;
using boost::find_if;
using boost::intrusive_ptr;
using boost::remove_erase_if;

using boost::intrusive_ptr;

template <typename T>
using intrusive_ref_counter =
    boost::intrusive_ref_counter<T, boost::thread_unsafe_counter>;

}  // namespace xlb::utils
