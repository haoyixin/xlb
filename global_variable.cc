#include <memory>
#include <atomic>

#include "config.h"
#include "dpdk.h"
#include "packet_pool.h"
#include "task.h"
#include "worker.h"

DEFINE_string(config, "../config.json", "Path of config file.");

namespace xlb {

// TODO: google style

std::atomic_flag dpdk_initialized = ATOMIC_FLAG_INIT;

bool Worker::quitting_;
__thread Worker Worker::current_ = {};

//std::shared_ptr<PacketPool> PacketPool::pools_[RTE_MAX_NUMA_NODES];

} // namespace xlb
