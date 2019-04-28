#pragma once

#include "runtime/common.h"
#include "runtime/packet_batch.h"
#include "runtime/worker.h"

namespace xlb {

// The weighted round robin scheduler.
// TODO: abstract
class Scheduler {
 public:
  explicit Scheduler();
  ~Scheduler() = default;

  // Runs the scheduler loop forever.
  template <Worker::Type type>
  void Loop();

  template <typename I, typename B>
  static double CpuUsage();

  // Functor used by a Worker's Scheduler to run a task in a module.
  class alignas(64) Task : public utils::INew {
   public:
    class alignas(64) Context {
     public:
      Context() = default;
      void Drop(Packet *pkt);
      void Hold(Packet *pkt);
      void Hold(PacketBatch *pkts);

      auto &stage_batch() { return stage_batch_; }

     private:
      Task *task_;
      uint64_t silent_drops_;

      // A packet batch for storing packets to free
      PacketBatch dead_batch_;
      PacketBatch stage_batch_;

      friend Scheduler;
      friend Task;
    };

    // TODO: better design ......
    struct Result {
      uint64_t packets;
    };

    using Func = std::function<Result(Context *)>;

    explicit Task(Func &&func, std::string_view name);
    ~Task();

   private:
    // TODO: tuning ......
    static constexpr int64_t kMinWeight = 4;
    static constexpr int64_t kMaxWeight = 1024;

    bool execute(Context *ctx) { return func_(ctx).packets == 0; }
    void update_weight(bool idle, uint64_t cycles);

    Func func_;
    // Just for debugging
    std::string name_;

    int64_t min_weight_;
    int64_t max_weight_;

    int64_t current_weight_;
    int64_t effective_weight_;
    bvar::Status<int64_t> display_weight_;

    alignas(64) Context context_;

    friend Scheduler;
  };

  void RegisterTask(Task::Func &&func, std::string_view name);

 private:
  using M = utils::Metric<TS("xlb_scheduler"), TS("cpu")>;
  using TaskQueue = utils::vector<Task *>;

  template <Worker::Type type>
  static void update_cpu_usage(bool idle, uint64_t cycles);

  Task::Context *next_ctx();

  TaskQueue runnable_;
  uint64_t checkpoint_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

template <typename I, typename B>
inline double Scheduler::CpuUsage() {
  auto idle = &M::PerSecond<I>();
  auto busy = &M::PerSecond<B>();

  // TODO: add a patch to brpc/bvar for fixing this
  double idle_ps = idle->get_value(idle->window_size());
  double busy_ps = busy->get_value(busy->window_size());

  return busy_ps / (busy_ps + idle_ps);
}

template <Worker::Type type>
inline void Scheduler::update_cpu_usage(bool idle, uint64_t cycles) {
  if (idle) {
    if constexpr (type == Worker::Slave)
      M::Adder<TS("idle_cycles_slaves")>() << cycles;
    else if constexpr (type == Worker::Master)
      M::Adder<TS("idle_cycles_master")>() << cycles;
    else if constexpr (type == Worker::Trivial)
      M::Adder<TS("idle_cycles_trivial")>() << cycles;
    else
      CHECK(0);
  } else {
    if constexpr (type == Worker::Slave)
      M::Adder<TS("busy_cycles_slaves")>() << cycles;
    else if constexpr (type == Worker::Master)
      M::Adder<TS("busy_cycles_master")>() << cycles;
    else if constexpr (type == Worker::Trivial)
      M::Adder<TS("busy_cycles_trivial")>() << cycles;
    else
      CHECK(0);
  }
}

}  // namespace xlb
