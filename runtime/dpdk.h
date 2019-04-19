#pragma once

#include "runtime/common.h"

namespace xlb {

// Initialize DPDK, with the specified amount of hugepage memory.
// Safe to call multiple times.
void InitDpdk();

} // namespace xlb
