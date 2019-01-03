#include "config.h"
#include "dpdk.h"
#include "opts.h"
#include "packet_pool.h"
#include "task.h"
#include "worker.h"

#include <memory>

DEFINE_string(config, "/mnt/haoyixin/CLionProjects/xlb/config.json",
              "Path of config file.");

namespace xlb {

// TODO: google style

bool is_initialized = false;

Config Config::all_;

//std::shared_ptr<Task::ProtoMap> Task::protos_;

//std::atomic<size_t> Worker::num_workers_;
//std::vector<std::thread> Worker::threads_;
bool Worker::quitting_;

__thread Worker Worker::current_ = {};

std::shared_ptr<PacketPool> PacketPool::pools_[RTE_MAX_NUMA_NODES];

} // namespace xlb
