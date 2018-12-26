#ifndef XLB_DPDK_H
#define XLB_DPDK_H

namespace xlb {

extern bool is_initialized;

// Initialize DPDK, with the specified amount of hugepage memory.
// Safe to call multiple times.
void InitDpdk(int dpdk_mb_per_socket);

} // namespace xlb

#endif // XLB_DPDK_H
