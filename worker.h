#ifndef XLB_WORKER_H
#define XLB_WORKER_H

#include "utils/common.h"
#include "utils/cuckoo_map.h"
#include "utils/random.h"
#include "utils/time.h"

#include <memory>
#include <thread>

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

  size_t id() { return id_; }
  size_t core() { return core_; }
  int socket() { return socket_; }

  Scheduler *scheduler() { return scheduler_; }
  PacketPool *packet_pool() { return packet_pool_; }
  utils::Random *random() const { return random_; }

  uint64_t silent_drops() { return silent_drops_; }
  uint64_t current_tsc() const { return current_tsc_; }
  uint64_t current_ns() const { return current_ns_; }

  static bool quitting() { return quitting_; }
  static Worker *current() { return &current_; }

  void IncrSilentDrops(uint64_t drops) { silent_drops_ += drops; }
  void UpdateTsc() {
    current_tsc_ = utils::Rdtsc();
    current_ns_ = utils::TscToNs(current_ns_);
  }

private:
  using Counter = std::atomic<size_t>;
  using Threads = std::vector<std::thread>;

  explicit Worker(size_t core);

  // The entry point of worker threads.
  void *Run();

  size_t id_;
  size_t core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  utils::Random *random_;

  uint64_t silent_drops_; // packets that have been sent to a deadend
  uint64_t current_tsc_;
  uint64_t current_ns_;

  static bool quitting_;
  //  static std::atomic<size_t> num_workers_;
  //  static std::vector<std::thread> threads_;

  static __thread Worker current_;
};

} // namespace xlb

#endif // XLB_WORKER_H
