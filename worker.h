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

  //  uint64_t silent_drops() const { return silent_drops_; }
  uint64_t current_tsc() const { return current_tsc_; }
  //  uint64_t current_ns() const { return current_ns_; }
  uint64_t busy_loops() const { return busy_loops_; }

  bool master() const { return master_; }

  static bool aborting() { return aborting_; }
  static bool slaves_aborted() { return slaves_aborted_; }
  static bool master_started() { return master_started_; }
  static Worker *current() { return &current_; }

  static void confirm_master() { master_started_ = true; }

  //  void IncrSilentDrops(uint64_t drops) { silent_drops_ += drops; }
  void UpdateTsc() {
    current_tsc_ = utils::Rdtsc();
    //    current_ns_ = utils::TscToNs(current_ns_);
  }
  void IncrBusyLoops() { ++busy_loops_; }

 private:
  explicit Worker(uint16_t core, bool master);

  // The entry point of worker threads.
  void *run();

  bool master_;

  uint16_t id_;
  uint16_t core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  utils::Random *random_;

  //  uint64_t silent_drops_;  // packets that have been sent to a deadend
  uint64_t current_tsc_;
  uint64_t busy_loops_;

  static bool aborting_;
  static bool slaves_aborted_;
  static bool master_started_;

  static std::atomic<uint16_t> counter_;
  static std::vector<std::thread> slave_threads_;
  static std::thread master_thread_;

  static __thread Worker current_;

  friend std::ostream &operator<<(std::ostream &os, const Worker &worker) {
    if (worker.master_)
      os << "[Master]";
    else
      os << "[Slave(" << worker.id_ << ")]";

    return os;
  }
};

#define W_CURRENT (Worker::current())

#define W_MASTER (W_CURRENT->master())
#define W_TSC (W_CURRENT->current_tsc())
#define W_ID (W_CURRENT->id())

#define W_LOG(severity) (LOG(severity) << *W_CURRENT << " ")

#if DCHECK_IS_ON()

#define W_DLOG(severity) (DLOG(severity) << *W_CURRENT << " ")
#define W_DVLOG(verboselevel) DVLOG(verboselevel) << *W_CURRENT << " "

#else

#define W_DLOG(severity) \
  true ? (void)0 : google::LogMessageVoidify() & LOG(severity)

#define W_DVLOG(verboselevel)         \
  (true || !VLOG_IS_ON(verboselevel)) \
      ? (void)0                       \
      : google::LogMessageVoidify() & LOG(INFO)

#endif

}  // namespace xlb
