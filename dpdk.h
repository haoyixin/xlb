#ifndef XLB_DPDK_H
#define XLB_DPDK_H

#include <atomic>

namespace xlb {

extern std::atomic_flag dpdk_initialized;

// Initialize DPDK, with the specified amount of hugepage memory.
// Safe to call multiple times.
void InitDpdk();

} // namespace xlb

#endif // XLB_DPDK_H
