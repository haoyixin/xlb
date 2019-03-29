#pragma once

#include <memory>
#include <thread>
#include <vector>

#include "utils/common.h"
#include "utils/random.h"
#include "utils/singleton.h"
#include "utils/time.h"

namespace xlb {

class Scheduler;

class PacketPool;

// class Task;

class Worker {
 public:
  // This is used for static ctor
  Worker() = default;

  static void Launch();
  static void Abort();
  static void Wait();

  uint16_t id() const { return id_; }
  uint16_t core() const { return core_; }
  int socket() const { return socket_; }

  Scheduler *scheduler() const { return scheduler_; }
  PacketPool *packet_pool() const { return packet_pool_; }
  utils::Random *random() const { return random_; }

  uint64_t silent_drops() const { return silent_drops_; }
  uint64_t current_tsc() const { return current_tsc_; }
  uint64_t current_ns() const { return current_ns_; }

  bool master() const { return master_; }

  static bool aborting() { return aborting_; }
  static bool slaves_aborted() { return slaves_aborted_; }
  static bool master_started() { return master_started_; }
  static Worker *current() { return &current_; }

  static void confirm_master() { master_started_ = true; }

  void IncrSilentDrops(uint64_t drops) { silent_drops_ += drops; }
  void UpdateTsc() {
    current_tsc_ = utils::Rdtsc();
    current_ns_ = utils::TscToNs(current_ns_);
  }

 private:
  using Slaves = utils::Singleton<std::vector<std::thread>, Worker>;
  using Master = utils::Singleton<std::thread, Worker>;
  using Counter = utils::Singleton<std::atomic<uint8_t>, Worker>;

  explicit Worker(uint16_t core, bool master);

  // The entry point of worker threads.
  void *run();
  //  void *run_master();

  bool master_;

  uint16_t id_;
  uint16_t core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  utils::Random *random_;

  uint64_t silent_drops_;  // packets that have been sent to a deadend
  uint64_t current_tsc_;
  uint64_t current_ns_;

  static bool aborting_;
  static bool slaves_aborted_;
  static bool master_started_;

  static __thread Worker current_;

  friend std::ostream &operator<<(std::ostream &os, const Worker &worker) {
    if (worker.master_)
      os << "[Master]";
    else
      os << "[Slave(" << worker.id_ << ")]";

    return os;
  }
};

#define DLOG_W(_L) DLOG(_L) << *(Worker::current()) << " "
#define LOG_W(_L) LOG(_L) << *(Worker::current()) << " "

}  // namespace xlb
