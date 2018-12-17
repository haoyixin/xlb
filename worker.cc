#include "worker.h"

#include <rte_lcore.h>

#include "module.h"
#include "scheduler.h"
#include "utils/numa.h"

namespace xlb {

// utils::CuckooMap<int, std::thread> Worker::threads_;
utils::CuckooMap<int, Worker> *Worker::workers_;

// std::shared_mutex Worker::mutex_;

__thread Worker *Worker::current_worker_;

void Worker::Destroy() { SetStatus(WORKER_QUITTING); }

void Worker::DestroyAll() {
  for (auto &[_, w] : *workers_) {
    w.Destroy();
  }
}

/* The entry point of worker threads */
void *Worker::Run() {
  random_ = new Random();
  rte_thread_set_affinity(&cpu_set_);

  /* DPDK lcore ID == worker ID (0, 1, 2, 3, ...) */
  RTE_PER_LCORE(_lcore_id) = core_;

  CHECK_GE(socket_, 0); // shouldn't be SOCKET_ID_ANY (-1)

  CHECK_NOTNULL(packet_pool_);

  current_worker_ = this;

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
  delete thread_;

  return nullptr;
}

Worker::Worker(int core)
    : core_(core), state_(WORKER_RUNNING), socket_(utils::core_to_socket(core_)),
      packet_pool_(PacketPool::GetPool(socket_)), silent_drops_(0),
      current_tsc_(0), current_ns_(0) {
  CPU_ZERO(&cpu_set_);
  CPU_SET(core_, &cpu_set_);
}

void Worker::Launch(int core) {
  if (!workers_)
    workers_ = new utils::CuckooMap<int, Worker>();

  auto worker = &workers_->Emplace(core, core)->second;
  worker->thread_ = new std::thread([=]() { worker->Run(); });
  worker->Thread()->detach();
}

} // namespace xlb
