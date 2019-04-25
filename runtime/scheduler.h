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
  static double CpuUsage() {
    auto idle = &M::PerSecond<I>();
    auto busy = &M::PerSecond<B>();

    // TODO: add a patch to brpc/bvar for fixing this
    double idle_ps = idle->get_value(idle->window_size());
    double busy_ps = busy->get_value(busy->window_size());

    return busy_ps / (busy_ps + idle_ps);
  }

  template <Worker::Type type>
  static void UpdateCpuUsage(bool idle, size_t cycles) {
    if constexpr (type == Worker::Slave) {
      if (idle)
        M::Adder<TS("idle_cycles_slaves")>() << cycles;
      else
        M::Adder<TS("busy_cycles_slaves")>() << cycles;
    } else if constexpr (type == Worker::Master) {
      if (idle)
        M::Adder<TS("idle_cycles_master")>() << cycles;
      else
        M::Adder<TS("busy_cycles_master")>() << cycles;
    } else if constexpr (type == Worker::Trivial) {
      if (idle)
        M::Adder<TS("idle_cycles_trivial")>() << cycles;
      else
        M::Adder<TS("busy_cycles_trivial")>() << cycles;
    } else {
      CHECK(0);
    }
  }

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

    /*
    struct Compare {
      bool operator()(Task *t1, Task *t2) const {
        return t1->current_weight_ < t2->current_weight_;
      }
    };
     */

    struct Result {
      uint64_t packets;
    };

    using Func = std::function<Result(Context *)>;

    explicit Task(Func &&func, uint32_t weight);

    ~Task();

   private:
    Func func_;

    int64_t current_weight_;
    uint32_t max_weight_;

    alignas(64) Context context_;

    friend Scheduler;
  };

  void RegisterTask(Task::Func &&func, uint32_t weight) {
    runnable_.emplace_back(new Task(std::move(func), weight));
  }

 private:
  using M = utils::Metric<TS("xlb_scheduler"), TS("cpu")>;
  //  using TaskQueue = utils::extended_priority_queue<Task *, Task::Compare>;
  using TaskQueue = utils::vector<Task *>;

  Task::Context *next_ctx();

  TaskQueue runnable_;
  //  TaskQueue *blocked_;

  uint64_t checkpoint_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

// TODO: ......

}  // namespace xlb
