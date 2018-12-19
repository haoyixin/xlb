#include "utils/numa.h"

#include "glog/logging.h"
#include "utils/format.h"
#include "utils/range.h"

#include <rte_lcore.h>

#include <experimental/filesystem>
#include <fstream>

namespace fs = std::experimental::filesystem;

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
bool core_present(int core_id) {
  return (core_id >= 0 && fs::exists(fs::path(Format(CORE_PATH, core_id))));
}

int num_sockets() {
  for (auto num : range(0, RTE_MAX_NUMA_NODES))
    if (!fs::exists(fs::path(Format(NUMA_PATH, num)))) {
      CHECK_NE(num, 0);
      return num;
    }
}

unsigned core_socket_id(unsigned core_id) {
  for (auto sid : range(0, RTE_MAX_NUMA_NODES))
    if (!fs::exists(fs::path(Format(CORE_SOCKET_PATH, core_id, sid)))) {
      CHECK_NE(sid, 0);
      return sid - 1;
    }
}

unsigned pci_socket_id(std::string &pci_addr) {
  auto p = Format(PCI_NUMA_PATH, pci_addr.c_str());
  CHECK(fs::exists(fs::path(p)));

  std::ifstream f(p);
  std::string l;
  int s;

  CHECK(f.is_open());
  CHECK(std::getline(f, l));

  CHECK_EQ(Parse(l, "%d", &s), 1);

  return s < 0 ? 0 : s;
}

} // namespace utils
} // namespace xlb
