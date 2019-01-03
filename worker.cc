#include "worker.h"

#include <glog/logging.h>
#include <rte_lcore.h>

#include "packet_pool.h"
#include "scheduler.h"
#include "utils/numa.h"
#include "utils/singleton.h"

namespace xlb {

/* The entry point of worker threads */
void *Worker::Run() {
  random_ = new utils::Random();
  rte_thread_set_affinity(&cpu_set_);

  /* DPDK lcore ID == worker ID (0, 1, 2, 3, ...) */
  //  RTE_PER_LCORE(_lcore_id) = core_;

  CHECK_GE(socket_, 0); // shouldn't be SOCKET_ID_ANY (-1)

  CHECK_NOTNULL(packet_pool_);

  //  current_worker_ = this;

  LOG(INFO) << "Worker "
            << "(" << this << ") "
            << "is running on core " << core_ << " (socket " << socket_ << ")";

  scheduler_ = new DefaultScheduler();
  scheduler_->ScheduleLoop();

  LOG(INFO) << "Worker "
            << "(" << this << ") "
            << "is quitting... (core " << core_ << ", socket " << socket_
            << ")";

  delete scheduler_;
  delete random_;

  return nullptr;
}

Worker::Worker(size_t core)
    : id_(utils::Singleton<Counter>::Get().fetch_add(1)), core_(core),
      socket_(utils::core_socket_id(core_)),
      packet_pool_(PacketPool::GetPool(socket_)), silent_drops_(0),
      current_tsc_(0), current_ns_(0) {
  CPU_ZERO(&cpu_set_);
  CPU_SET(core_, &cpu_set_);
}

void Worker::Launch() {
  for (auto core : CONFIG.worker_cores)
    utils::Singleton<Threads>::Get()
        .emplace_back([=]() { (new (current()) Worker(core))->Run(); })
        .detach();
}

void Worker::Quit() {
  quitting_ = true;
  for (auto &thread : utils::Singleton<Threads>::Get())
    thread.join();
}

} // namespace xlb
