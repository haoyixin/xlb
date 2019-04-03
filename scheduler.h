#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

#include "utils/common.h"
#include "utils/extended_priority_queue.h"
#include "utils/metric.h"
#include "utils/singleton.h"
#include "utils/time.h"

#include "packet_batch.h"
#include "worker.h"

namespace xlb {

// The weighted round robin scheduler.
// TODO: abstract
class Scheduler {
 public:
  explicit Scheduler();
  virtual ~Scheduler();

  // Runs the scheduler loop forever.
  void MasterLoop();
  void SlaveLoop();

  template <typename I, typename B>
  static double CpuUsage() {
    auto idle = &M::PerSecond<I>();
    auto busy = &M::PerSecond<B>();

    // TODO: add a patch to brpc/bvar for fixing this
    double idle_ps = idle->get_value(idle->window_size());
    double busy_ps = busy->get_value(busy->window_size());

    return busy_ps / (busy_ps + idle_ps);
  }

  // Functor used by a Worker's Scheduler to run a task in a module.
  class Task {
   public:
    class Context {
     public:
      static constexpr size_t kMaxCounters = 8;

      Context() = default;
      void Drop(Packet *pkt);
      void Hold(Packet *pkt);
      void Hold(PacketBatch *pkts);

      auto worker() { return worker_; }
      auto &stage_batch() { return stage_batch_; }
//      auto &counters() { return counters_; }

     private:
      Task *task_;
      Worker *worker_;
      uint64_t silent_drops_;

      // A packet batch for storing packets to free
      PacketBatch dead_batch_;
      PacketBatch stage_batch_;

      //  User-defined counters
//      alignas(32) std::array<uint64_t, kMaxCounters> counters_;

      friend Scheduler;
      friend Task;
    };

    struct Compare {
      bool operator()(Task *t1, Task *t2) const {
        return t1->current_weight_ < t2->current_weight_;
      }
    };

    struct Result {
      uint64_t packets;
    };

    using Func = std::function<Result(Context *)>;

   protected:
    explicit Task(Func &&func, uint8_t weight);

    ~Task();

   private:
    Func func_;
    Context context_;

    uint8_t current_weight_;
    uint8_t max_weight_;

    friend Scheduler;
  };

  void RegisterTask(Task::Func &&func, uint8_t weight) {
    runnable_->emplace(new Task(std::move(func), weight));
  }

 private:
  using M = utils::Metric<TS("xlb_scheduler"), TS("cpu")>;
  using TaskQueue = utils::extended_priority_queue<Task *, Task::Compare>;

  Task::Context *next_ctx();

  TaskQueue *runnable_;
  TaskQueue *blocked_;

  uint64_t checkpoint_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace xlb
