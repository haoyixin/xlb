#include "worker.h"

#include <glog/logging.h>
#include <pthread.h>
#include <rte_lcore.h>

#include "utils/format.h"
#include "utils/numa.h"
#include "utils/singleton.h"

#include "packet_pool.h"
#include "scheduler.h"

namespace xlb {

__thread Worker Worker::current_ = {};
bool Worker::aborting_;

// The entry point of worker threads
void *Worker::Run() {
  random_ = new utils::Random();

  auto name = utils::Format("worker-%u@%u", id_, core_);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_), &cpu_set_);
  pthread_setname_np(pthread_self(), name.c_str());

  // DPDK lcore ID == worker ID (0, 1, 2, 3, ...)
  RTE_PER_LCORE(_lcore_id) = id_;

  // shouldn't be SOCKET_ID_ANY (-1)
  CHECK_GE(socket_, 0);
  CHECK_NOTNULL(packet_pool_);

  LOG(INFO) << "Worker "
            << "(" << id_ << ") "
            << "is running on core " << core_ << " (socket " << socket_ << ")";

  scheduler_ = new Scheduler();
  scheduler_->Loop();

  LOG(INFO) << "Worker "
            << "(" << id_ << ") "
            << "is quitting... (core " << core_ << ", socket " << socket_
            << ")";

  delete scheduler_;
  delete random_;

  return nullptr;
}

Worker::Worker(uint16_t core)
    : id_(utils::Singleton<Counter, Worker>::instance().fetch_add(1)),
      core_(core),
      socket_(CONFIG.nic.socket),
      packet_pool_(&utils::Singleton<PacketPool>::instance()),
      silent_drops_(0),
      current_tsc_(0),
      current_ns_(0) {
  CPU_ZERO(&cpu_set_);
  CPU_SET(core_, &cpu_set_);
}

void Worker::Launch() {
  for (auto core : CONFIG.worker_cores)
    utils::Singleton<Threads, Worker>::instance().emplace_back(
        [=]() { (new (&current_) Worker(core))->Run(); });
}

void Worker::Abort() { aborting_ = true; }

void Worker::Wait() {
  for (auto &thread : utils::Singleton<Threads, Worker>::instance())
    if (thread.joinable()) thread.join();

  rte_eal_mp_wait_lcore();
}

}  // namespace xlb
