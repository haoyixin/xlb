#ifndef XLB_WORKER_H
#define XLB_WORKER_H

#include "utils/common.h"
#include "utils/cuckoo_map.h"
#include "utils/random.h"
#include "utils/time.h"

#include <thread>

namespace xlb {

class Scheduler;
class PacketPool;
class Task;

class Worker {
public:
  using Map = utils::CuckooMap<int, Worker>;

  typedef enum {
    RUNNING = 0,
    QUITTING,
  } State;

  Worker() = default;
  explicit Worker(int core);

  State state() { return state_; }
  int id() { return id_; }
  int core() { return core_; }
  int socket() { return socket_; }
  Scheduler *scheduler() { return scheduler_; }
  PacketPool *packet_pool() { return packet_pool_; }
  Random *random() const { return random_; }
  std::thread *thread() const { return thread_; }

  uint64_t silent_drops() { return silent_drops_; }
  void set_silent_drops(uint64_t drops) { silent_drops_ = drops; }
  void incr_silent_drops(uint64_t drops) { silent_drops_ += drops; }
  uint64_t current_tsc() const { return current_tsc_; }
  uint64_t current_ns() const { return current_ns_; }

  void update_tsc() {
    current_tsc_ = rdtsc();
    current_ns_ = tsc_to_ns(current_ns_);
  }

  static void Launch(int core);
  static void DestroyAll();
  static Worker *current() { return current_worker_; }

private:
  // The entry point of worker threads.
  void *Run();

  // There is currently no need to modify the worker state from the outside,
  // except for DestroyAll and Launch.
  void Destroy();
  void set_state(State state) { state_ = state; }

  State state_;
  int id_;
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
  static Map *workers_;
  static std::atomic<int> num_workers_;

  //  static std::shared_mutex mutex_;
  static __thread Worker *current_worker_;
};

} // namespace xlb

#endif // XLB_WORKER_H
