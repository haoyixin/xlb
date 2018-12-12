#ifndef XLB_UTILS_NUMA_H
#define XLB_UTILS_NUMA_H

#define NUMA_NODE_PATH "/sys/devices/system/node"
#define SYS_CPU_DIR "/sys/devices/system/cpu/cpu%u"
#define CORE_ID_FILE "topology/core_id"

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
bool IsValidCore(int core_id);

int NumNumaNodes();

unsigned cpu_socket_id(unsigned lcore_id);

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_NUMA_H
