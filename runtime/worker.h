#pragma once

#include "runtime/common.h"

namespace xlb {

class Scheduler;
class PacketPool;

class Worker {
 public:
  enum Type { Master = 0, Slave = 1, Trivial = 2 };
  // This is used for static ctor
  Worker() = default;

  static void Launch();
  static void Abort();
  static void Wait();

  uint16_t id() const { return id_; }
  uint16_t core() const { return core_; }
  int socket() const { return socket_; }
  Type type() const { return type_; }
  std::string type_string() const;

  Scheduler *scheduler() const { return scheduler_; }
  PacketPool *packet_pool() const { return packet_pool_; }
  utils::Random *random() const { return random_; }

  uint64_t current_tsc() const { return current_tsc_; }
  uint64_t busy_loops() const { return busy_loops_; }

  static Worker *current() { return &current_; }

  void UpdateTsc() { current_tsc_ = utils::Rdtsc(); }
  void IncrBusyLoops() { ++busy_loops_; }

  template <Type type>
  static void MarkStarted();
  template <Type type>
  static void MarkAborted();

  template <Type type>
  static bool starting() {
    return internal<type>::starting_.load(std::memory_order_consume);
  }
  template <Type type>
  static bool aborting() {
    return internal<type>::aborting_.load(std::memory_order_consume);
  }

 private:
  explicit Worker(uint16_t core, Type type);

  // The entry point of worker threads.
  template <Type type>
  void *run();

  Type type_;

  uint16_t id_;
  uint16_t core_;
  int socket_;
  cpu_set_t cpu_set_;

  PacketPool *packet_pool_;
  Scheduler *scheduler_;
  utils::Random *random_;

  uint64_t current_tsc_;
  uint64_t busy_loops_;

  template <Type type>
  struct internal {
    static std::atomic<bool> starting_;
    static std::atomic<bool> aborting_;
  };

  static std::vector<std::thread> slave_threads_;
  static std::thread master_thread_;
  static std::thread trivial_thread_;

  static __thread Worker current_;

  template <Type type>
  static constexpr const char *type_str() {
    if constexpr (type == Slave)
      return "slave";
    else if constexpr (type == Master)
      return "master";
    else if constexpr (type == Trivial)
      return "trivial";
  }

  friend std::ostream &operator<<(std::ostream &os, const Worker &worker) {
    return os << "[" << worker.type_string() << "(" << worker.id() << ")]";
  }
};

template <Worker::Type type>
std::atomic<bool> Worker::internal<type>::starting_ = false;
template <Worker::Type type>
std::atomic<bool> Worker::internal<type>::aborting_ = false;

#define W_CURRENT (Worker::current())

#define W_SLAVE (W_CURRENT->type() == Worker::Slave)
#define W_TYPE_STR (W_CURRENT->type_string().c_str())
#define W_TSC (W_CURRENT->current_tsc())
#define W_ID (W_CURRENT->id())

#define W_LOG(severity) \
  (LOG(severity) << *W_CURRENT << " [" << __FUNCTION__ << "] ")

#define W_LOG_EVERY_SECOND(severity) \
  LOG_EVERY_SECOND(severity) << *W_CURRENT << " [" << __FUNCTION__ << "] "

#if DCHECK_IS_ON()

#define W_DLOG(severity) \
  (DLOG(severity) << *W_CURRENT << " [" << __FUNCTION__ << "] ")
#define W_DVLOG(verboselevel) \
  DVLOG(verboselevel) << *W_CURRENT << " [" << __FUNCTION__ << "] "

#else

#define W_DLOG(severity) \
  true ? (void)0 : google::LogMessageVoidify() & LOG(severity)

#define W_DVLOG(verboselevel)         \
  (true || !VLOG_IS_ON(verboselevel)) \
      ? (void)0                       \
      : google::LogMessageVoidify() & LOG(INFO)

#endif

}  // namespace xlb
