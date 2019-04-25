#include "runtime/worker.h"
#include "runtime/packet_pool.h"
// TODO: ......
#include "runtime/exec.h"
#include "runtime/module.h"

namespace xlb {

__thread Worker Worker::current_ = {};

std::vector<std::thread> Worker::slave_threads_ = {};
std::thread Worker::master_thread_ = {};
std::thread Worker::trivial_thread_ = {};

std::string Worker::type_string() const {
  if (type_ == Slave)
    return "slave";
  else if (type_ == Master)
    return "master";
  else if (type_ == Trivial)
    return "trivial";
  else
    CHECK(0);
}

template <>
void Worker::MarkStarted<Worker::Master>() {
  internal<Slave>::starting_.store(true, std::memory_order_release);
}

template <>
void Worker::MarkStarted<Worker::Slave>() {
  static std::atomic<uint16_t> counter = 0;
  if (counter.fetch_add(1) + 1 == CONFIG.slave_cores.size()) {
    W_LOG(INFO) << "All slaves have been started";
    internal<Trivial>::starting_.store(true, std::memory_order_release);
  }
}

template <>
void Worker::MarkStarted<Worker::Trivial>() {}

template <>
void Worker::MarkAborted<Worker::Trivial>() {
  internal<Slave>::aborting_.store(true, std::memory_order_release);
}

template <>
void Worker::MarkAborted<Worker::Slave>() {
  static std::atomic<uint16_t> counter = 0;
  if (counter.fetch_add(1) + 1 == CONFIG.slave_cores.size()) {
    W_LOG(INFO) << "All slaves have been aborted";
    internal<Master>::aborting_.store(true, std::memory_order_release);
  }
}

template <>
void Worker::MarkAborted<Worker::Master>() {}

Worker::Worker(uint16_t core, Type type)
    : type_(type),
      core_(core),
      packet_pool_(&utils::Singleton<PacketPool>::instance()),
      current_tsc_(utils::Rdtsc()) {
  static std::atomic<uint16_t> counter = 0;
  switch (type_) {
    case Slave:
      id_ = counter.fetch_add(1);
      socket_ = CONFIG.nic.socket;
      break;
    case Master:
      id_ = CONFIG.slave_cores.size();
      socket_ = utils::CoreSocketId(core_).value();
      break;
    case Trivial:
      id_ = CONFIG.slave_cores.size() + 1;
      socket_ = utils::CoreSocketId(core_).value();
      break;
    default:
      CHECK(0);
      break;
  }

  CPU_ZERO(&cpu_set_);
  CPU_SET(core_, &cpu_set_);
}

void Worker::Launch() {
  master_thread_ = std::thread([=]() {
    (new (&current_) Worker(CONFIG.master_core, Master))->run<Master>();
  });

  for (auto core : CONFIG.slave_cores)
    slave_threads_.emplace_back(
        [=]() { (new (&current_) Worker(core, Slave))->run<Slave>(); });

  trivial_thread_ = std::thread([=]() {
    (new (&current_) Worker(CONFIG.trivial_core, Trivial))->run<Trivial>();
  });

  internal<Master>::starting_.store(true, std::memory_order_release);
}

void Worker::Abort() {
  internal<Trivial>::aborting_.store(true, std::memory_order_release);
}

void Worker::Wait() {
  trivial_thread_.join();

  for (auto &thread : slave_threads_) thread.join();

  master_thread_.join();

  rte_eal_mp_wait_lcore();
}

}  // namespace xlb
