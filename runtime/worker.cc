#include "runtime/worker.h"
#include "runtime/packet_pool.h"
// TODO: ......
#include "runtime/exec.h"
#include "runtime/module.h"

namespace xlb {

__thread Worker Worker::current_ = {};

bool Worker::aborting_ = false;
bool Worker::slaves_aborted_ = false;
bool Worker::master_started_ = false;

std::atomic<uint16_t> Worker::counter_ = 0;
std::vector<std::thread> Worker::slave_threads_ = {};
std::thread Worker::master_thread_ = {};
std::thread Worker::trivial_thread_ = {};

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

  W_LOG(INFO) << "Starting on core: " << core_ << " socket: " << socket_;

  scheduler_ = new Scheduler();

  if (!master_)
    scheduler_->Loop<false>();
  else
    scheduler_->Loop<true>();

  W_LOG(INFO) << "Quitting on core: " << core_ << " socket: " << socket_;

  if (!master_ && (counter_.fetch_sub(1) == 1)) {
    W_LOG(INFO) << "All slaves have been aborted";
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
  trivial_thread_ = std::thread([]() {
    /*
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(CONFIG.trivial_core, &cpu_set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
     */

    Exec::RegisterTrivial();

    for (;;) {
      if (slaves_aborted() && Exec::Sync() == 0) break;

      Exec::Sync();
      usleep(1000000 / CONFIG.rpc.max_concurrency);
    }
  });

  master_thread_ = std::thread(
      [=]() { (new (&current_) Worker(CONFIG.master_core, true))->run(); });


  for (auto core : CONFIG.slave_cores)
    slave_threads_.emplace_back(
        [=]() { (new (&current_) Worker(core, false))->run(); });
}

void Worker::Abort() { aborting_ = true; }

void Worker::Wait() {
  for (auto &thread : slave_threads_) thread.join();

  master_thread_.join();
  trivial_thread_.join();
  rte_eal_mp_wait_lcore();
}

}  // namespace xlb
