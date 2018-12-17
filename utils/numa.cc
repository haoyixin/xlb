#include "utils/numa.h"

#include "glog/logging.h"
#include "utils/format.h"
#include "utils/range.h"

#include <rte_lcore.h>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
bool is_valid_core(int core_id) {
  return (core_id >= 0 &&
          fs::exists(fs::path(Format("%s%u", SYS_CPU_DIR, core_id))));
}

int num_sockets() {
  for (auto num : range(0, RTE_MAX_NUMA_NODES))
    if (!fs::exists(fs::path(Format("%s%u", NUMA_NODE_PATH, num))))
      if (num == 0)
        LOG(FATAL) << "NUMA support is not available";
      else
        return num;
}

unsigned core_to_socket(unsigned lcore_id) {
  for (auto sid : range(0, RTE_MAX_NUMA_NODES))
    if (!fs::exists(
            fs::path(Format("%s%u/node%u", SYS_CPU_DIR, lcore_id, sid))))
      if (sid = 0)
        LOG(FATAL) << "NUMA support is not available";
      else
        return sid - 1;
}

} // namespace utils
} // namespace xlb
