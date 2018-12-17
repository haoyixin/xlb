#ifndef XLB_UTILS_NUMA_H
#define XLB_UTILS_NUMA_H

#define NUMA_NODE_PATH "/sys/devices/system/node/node"
#define SYS_CPU_DIR "/sys/devices/system/cpu/cpu"
#define SOCKET_ID_FILE "topology/physical_package_id"

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
bool is_valid_core(int core_id);

int num_sockets();

unsigned core_to_socket(unsigned lcore_id);

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_NUMA_H
