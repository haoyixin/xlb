#pragma once

#include <atomic>

namespace xlb {

// Initialize DPDK, with the specified amount of hugepage memory.
// Safe to call multiple times.
void InitDpdk();

} // namespace xlb
