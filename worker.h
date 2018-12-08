#ifndef XLB_WORKER_H
#define XLB_WORKER_H

#include <glog/logging.h>

#include <cstdint>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>

#include "utils/common.h"
#include "utils/cuckoo_map.h"
#include "utils/random.h"

namespace xlb {

typedef enum {
  WORKER_RUNNING = 0,
  WORKER_QUITTING,
} WorkerState;

class Scheduler;
class PacketPool;

class Task;

class Worker {
public:
  Worker() = default;
  explicit Worker(int core);

  WorkerState State() { return state_; }
  int Core() { return core_; }
  int Socket() { return socket_; }
  Scheduler *Sched() { return scheduler_; }
  PacketPool *PktPool() { return packet_pool_; }
  Random *Rand() const { return random_; }
  std::thread *Thread() const { return thread_; }

  uint64_t silent_drops() { return silent_drops_; }
  void set_silent_drops(uint64_t drops) { silent_drops_ = drops; }
  void incr_silent_drops(uint64_t drops) { silent_drops_ += drops; }
  uint64_t current_tsc() const { return current_tsc_; }
  void set_current_tsc(uint64_t tsc) { current_tsc_ = tsc; }
  uint64_t current_ns() const { return current_ns_; }
  void set_current_ns(uint64_t ns) { current_ns_ = ns; }

  static void Launch(int core);
  static void DestroyAll();
  static Worker *Current() { return current_worker_; }

private:
  // The entry point of worker threads.
  void *Run();

  // There is currently no need to modify the worker state from the outside,
  // except for DestroyAll and Launch.
  void Destroy();
  void SetStatus(WorkerState state) { state_ = state; }

  WorkerState state_;
  int core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  Random *random_;
  std::thread *thread_;

  uint64_t silent_drops_; // packets that have been sent to a deadend
  uint64_t current_tsc_;
  uint64_t current_ns_;

  //  static utils::CuckooMap<int, std::thread *> *threads_;
  static utils::CuckooMap<int, Worker> *workers_;

  //  static std::shared_mutex mutex_;
  static __thread Worker *current_worker_;
};

} // namespace xlb

#endif // XLB_WORKER_H
