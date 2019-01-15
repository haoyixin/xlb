#ifndef XLB_WORKER_H
#define XLB_WORKER_H

#include <memory>
#include <thread>

#include "utils/common.h"
#include "utils/cuckoo_map.h"
#include "utils/random.h"
#include "utils/time.h"

namespace xlb {

class Scheduler;

class PacketPool;

class Task;

class Worker {
public:
  // This is used for static ctor
  Worker() = default;

  static void Launch();
  static void Quit();
  static void Wait();

  uint16_t id() { return id_; }
  uint16_t core() { return core_; }
  int socket() { return socket_; }

  Scheduler *scheduler() { return scheduler_; }
  PacketPool *packet_pool() { return packet_pool_; }
  utils::Random *random() { return random_; }

  uint64_t silent_drops() { return silent_drops_; }
  uint64_t current_tsc() { return current_tsc_; }
  uint64_t current_ns() { return current_ns_; }

  static bool quitting() { return quitting_; }
  static Worker *current() { return &current_; }

  void IncrSilentDrops(uint64_t drops) { silent_drops_ += drops; }
  void UpdateTsc() {
    current_tsc_ = utils::Rdtsc();
    current_ns_ = utils::TscToNs(current_ns_);
  }

private:
  using Counter = std::atomic<uint16_t>;
  using Threads = std::vector<std::thread>;

  explicit Worker(uint16_t core);

  // The entry point of worker threads.
  void *Run();

  uint16_t id_;
  uint16_t core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  utils::Random *random_;

  uint64_t silent_drops_; // packets that have been sent to a deadend
  uint64_t current_tsc_;
  uint64_t current_ns_;

  static bool quitting_;

  static __thread Worker current_;
};

} // namespace xlb

#endif // XLB_WORKER_H
