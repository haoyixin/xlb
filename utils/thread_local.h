#ifndef XLB_UTILS_RCU_H
#define XLB_UTILS_RCU_H

#include <functional>
#include <shared_mutex>

#include <bthread/bthread.h>

#include "utils/lock_less_queue.h"
#include "utils/range.h"

#include "cuckoo_map.h"

namespace xlb {
namespace utils {

template <typename T, typename R> class alignas(64) ThreadLocal {
private:
  using MapResults = CuckooMap<size_t, R>;
  using VoidFunc = std::function<void(T *)>;
  using MapFunc = std::function<void(T *, R *)>;
  using MergeFunc = std::function<void(const MapResults *)>;
  using SafeReadFunc = std::function<void(const T *)>;

  class TaskBase {
  public:
    TaskBase(ThreadLocal *data)
        : data_(data), count_done_(data->num_threads()) {}

    virtual ~TaskBase() {}

    void Run() {
      run();
      if (last())
        done();
    }

  protected:
    virtual void run() {}
    virtual void done() {}

  private:
    bool last() { return count_done_.fetch_sub(1) - 1 == 0; }
    ThreadLocal *data_;
    std::atomic<size_t> count_done_;
  };

  class VoidTask final : public TaskBase {
  public:
    VoidTask(ThreadLocal *data, VoidFunc void_func)
        : TaskBase(data), void_func_(void_func) {}

  protected:
    void run() override { void_func_(data_); }
    void done() override { delete this; }

  private:
    VoidFunc void_func_;
  };

  class MapTask final : public TaskBase {
  public:
    MapTask(ThreadLocal *data, MapFunc map_func, MergeFunc merge_func)
        : TaskBase(data), map_func_(map_func), merge_func_(merge_func) {
      results_ = new MapResults(data->socket());
      for (size_t tid = 0; tid < kMaxThread; tid++)
        if (data->data_pointer_[tid])
          results_->Emplace(tid);

//      bthread_mutex_init(&merge_cond_mutex_, nullptr);
//      bthread_cond_init(&merge_cond_, nullptr);
//      bthread_start_background(&merge_thread_, nullptr, merge_thread_func,
//                               this);
    }

    ~MapTask() {
      delete results_;
//      bthread_mutex_destroy(&merge_cond_mutex_);
//      bthread_cond_destroy(&merge_cond_);
      bthread_join(&merge_thread_, nullptr);
    }

  protected:
    void run() {
      map_func_(data_->Data(), &results_->Find(data_->tid())->second);
    }

    void done() {
        bthread_start_background(&merge_thread_, nullptr, merge_thread_func,
                                 this);
//      bthread_mutex_lock(&merge_cond_mutex_);
//      bthread_cond_signal(&merge_cond_);
//      bthread_mutex_unlock(&merge_cond_mutex_);
    }

  private:
    static void *merge_thread_func(void *task) {
      auto t = static_cast<MapTask *>(task);

      bthread_mutex_lock(&t->merge_cond_mutex_);
      while (t->count_done_.load() != 0)
        bthread_cond_wait(&t->merge_cond_, &t->merge_cond_mutex_);

      bthread_mutex_unlock(&t->merge_cond_mutex_);

      t->merge_func_(&t->results_);

      delete t;
      bthread_exit(nullptr);
    }

    bthread_mutex_t merge_cond_mutex_;
    bthread_cond_t merge_cond_;
    bthread_t merge_thread_;

    MapFunc map_func_;
    MergeFunc merge_func_;

    MapResults *results_;
  };

public:
  // This must be big enough, we are not currently dealing with queue full
  static const size_t kDefaultOpsLength = 4096;
  static const size_t kMaxThread = 32;

  template <typename... Args>
  explicit ThreadLocal(size_t num_threads, int socket = SOCKET_ID_ANY,
                       Args &&... args)
      : socket_(socket), data_pointer_(), task_queue_pointer_(), data_(),
        task_queue_(), registered_num_(0) {
    CHECK_LE(num_threads, kMaxThread);

    for (auto i : range(0, num_threads)) {
      new (data_ + i) T(std::forward<Args>(args)...);
      new (task_queue_ + i)
          LockLessQueue<TaskBase *>(socket_, kDefaultOpsLength, false, true);
    }
    // 0 is reserved
    Register(0);
  }

  // Best case is core equal to tid
  void Register(size_t tid) {
    // If our thread didn't call this, tid will be zero (for default usage)
    CHECK_GE(tid, 0);
    tid_ = tid;

    auto offset = registered_num_.fetch_add(1);
    data_pointer_[tid] = &data_[offset];
    task_queue_pointer_[tid] = &task_queue_[offset];
  }

  void Map(VoidFunc void_func) {
    for (auto &queue : task_queue_)
      queue.Push(new VoidTask(this, void_func));
  }

  void MapAndMerge(MapFunc map_func, MergeFunc merge_func) {
    for (auto &queue : task_queue_)
      queue.Push(new MapTask(this, map_func, merge_func));
  }

  // This will hardly fail, but there may be places that are not well thought
  // out.
  void Sync() {
    for (;;) {
      TaskBase *task;
      if (task_queue_pointer_[tid_]->Pop(task) != 0)
        break;

      task->Run();
    }
  }

  // TODO: safe should not be here

  void SyncSafe() {
    std::lock_guard<std::shared_mutex> guard(rwlock_);
    Sync();
  }

  void ReadSafe(SafeReadFunc safe_read_func) {
    std::shared_lock<std::shared_mutex> guard(rwlock_);
    safe_read_func(const_cast<const T *>(unsafe_data()));
  }

  void WriteSafe(VoidFunc void_func) {
    std::lock_guard<std::shared_mutex> guard(rwlock_);
    void_func(unsafe_data());
  }

  // TODO: --------------------------------

  size_t num_threads() { return registered_num_.load(); }

  // Unsafe data for performance, this should not call with tid = 0
  T *unsafe_data() { return data_pointer_[tid()]; }

  int socket() { return socket_; }

  static size_t tid() { return tid_; }

private:
  int socket_;

  alignas(64) T *data_pointer_[kMaxThread];
  T data_[kMaxThread];

  alignas(64) LockLessQueue<TaskBase *> *task_queue_pointer_[kMaxThread];
  LockLessQueue<TaskBase *> task_queue_[kMaxThread];

  std::atomic<size_t> registered_num_;
  std::shared_mutex rwlock_;

  static __thread size_t tid_;
};

template <typename T, typename R> __thread size_t ThreadLocal<T, R>::tid_ = {};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_RCU_H
