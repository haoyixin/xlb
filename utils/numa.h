#ifndef XLB_UTILS_NUMA_H
#define XLB_UTILS_NUMA_H

#include <string>

#define CORE_PATH "/sys/devices/system/cpu/cpu%u"
#define NUMA_PATH "/sys/devices/system/node/node%u"
#define CORE_SOCKET_PATH "/sys/devices/system/cpu/cpu%u/node%u"
#define PCI_NUMA_PATH "/sys/bus/pci/devices/%s/numa_node"

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
bool core_present(int core_id);

int num_sockets();

unsigned core_socket_id(unsigned core_id);

unsigned pci_socket_id(std::string &pci_addr);

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_NUMA_H
