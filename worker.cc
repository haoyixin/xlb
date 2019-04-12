#include "worker.h"

#include <glog/logging.h>
#include <pthread.h>
#include <rte_lcore.h>

#include "utils/common.h"
#include "utils/format.h"
#include "utils/numa.h"
#include "utils/singleton.h"
#include "utils/time.h"

#include "packet_pool.h"

// TODO: ......
#include "module.h"

namespace xlb {

__thread Worker Worker::current_ = {};

bool Worker::aborting_ = false;
bool Worker::slaves_aborted_ = false;
bool Worker::master_started_ = false;

std::atomic<uint16_t> Worker::counter_ = 0;
std::vector<std::thread> Worker::slave_threads_ = {};
std::thread Worker::master_thread_ = {};

// The entry point of worker threads
void *Worker::run() {
  random_ = new utils::Random();

  std::string name;

  if (!master_)
    name = utils::Format("worker@%u", core_);
  else
    name = utils::Format("master@%u", core_);

  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_), &cpu_set_);
  pthread_setname_np(pthread_self(), name.c_str());

  // DPDK lcore ID == worker ID (0, 1, 2, 3, ...)
  RTE_PER_LCORE(_lcore_id) = id_;

  // shouldn't be SOCKET_ID_ANY (-1)
  CHECK_GE(socket_, 0);
  CHECK_NOTNULL(packet_pool_);

  if (!master_)
    while (!master_started_) INST_BARRIER();

  W_LOG(INFO) << "[Worker] Starting on core: " << core_
              << " socket: " << socket_;

  scheduler_ = new Scheduler();

  if (!master_)
    scheduler_->Loop<false>();
  else
    scheduler_->Loop<true>();

  W_LOG(INFO) << "[Worker] Quitting on core: " << core_
              << " socket: " << socket_;

  if (!master_ && (counter_.fetch_sub(1) == 1)) {
    W_LOG(INFO) << "[Worker] All slaves have been aborted";
    slaves_aborted_ = true;
  }

  delete scheduler_;
  delete random_;

  return nullptr;
}

Worker::Worker(uint16_t core, bool master)
    : master_(master),
      core_(core),
      packet_pool_(&utils::Singleton<PacketPool>::instance()),
      current_tsc_(utils::Rdtsc()) {
  if (!master_) {
    id_ = counter_.fetch_add(1);
    socket_ = CONFIG.nic.socket;
  } else {
    id_ = std::numeric_limits<decltype(id_)>::max();
    socket_ = utils::CoreSocketId(core_).value();
  }

  CPU_ZERO(&cpu_set_);
  CPU_SET(core_, &cpu_set_);
}

void Worker::Launch() {
  master_thread_ = std::thread(
      [=]() { (new (&current_) Worker(CONFIG.master_core, true))->run(); });

  for (auto core : CONFIG.slave_cores)
    slave_threads_.emplace_back(
        [=]() { (new (&current_) Worker(core, false))->run(); });
}

void Worker::Abort() { aborting_ = true; }

void Worker::Wait() {
  for (auto &thread : slave_threads_)
    if (thread.joinable()) thread.join();

  if (master_thread_.joinable()) master_thread_.join();

  rte_eal_mp_wait_lcore();
}

}  // namespace xlb
