#pragma once

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/range/irange.hpp>
//#include <boost/range/begin.hpp>
//#include <boost/range/end.hpp>
#include <boost/range/algorithm/find_if.hpp>

namespace xlb {
namespace utils {

using boost::algorithm::any_of;
using boost::algorithm::any_of_equal;

using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::algorithm::trim;
using boost::irange;

//using boost::begin;
//using boost::end;
using boost::find_if;

} // namespace utils
} // namespace xlb
