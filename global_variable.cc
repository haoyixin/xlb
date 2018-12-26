#include "config.h"
#include "dpdk.h"
#include "opts.h"
#include "packet_pool.h"
#include "task.h"
#include "worker.h"

DEFINE_string(config, "/mnt/haoyixin/CLionProjects/xlb/config.json",
              "Path of config file.");

namespace xlb {

bool is_initialized = false;

Config Config::all_;

Task::ProtoMap *Task::protos_;

Worker::Map *Worker::workers_;
std::atomic<int> Worker::num_workers_;
__thread Worker *Worker::current_worker_;

PacketPool *PacketPool::pools_[RTE_MAX_NUMA_NODES];

} // namespace xlb
