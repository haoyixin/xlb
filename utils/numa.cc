#include "utils/numa.h"

#include "utils/format.h"
#include <fstream>
#include <limits.h>
#include <rte_lru.h>
#include <unistd.h>

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
int is_cpu_present(unsigned int core_id) {
  char path[PATH_MAX];
  int len = snprintf(path, sizeof(path), SYS_CPU_DIR "/" CORE_ID_FILE, core_id);
  if (len <= 0 || (unsigned)len >= sizeof(path)) {
    return 0;
  }
  if (access(path, F_OK) != 0) {
    return 0;
  }

  return 1;
}

int NumNumaNodes() {
  static int cached = 0;
  if (cached > 0) {
    return cached;
  }

  std::ifstream fp("/sys/devices/system/node/possible");
  if (fp.is_open()) {
    std::string line;
    if (std::getline(fp, line)) {
      int cnt;
      if (Parse(line, "0-%d", &cnt) == 1) {
        cached = cnt + 1;
        return cached;
      }
    }
  }
}

unsigned cpu_socket_id(unsigned lcore_id) {
  unsigned socket;

  for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/node%u/cpu%u", NUMA_NODE_PATH, socket,
             lcore_id);
    if (access(path, F_OK) == 0)
      return socket;
  }
  return 0;
}

} // namespace utils
} // namespace xlb
