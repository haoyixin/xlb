#pragma once

#include <shared_mutex>
#include <vector>

#include <glog/logging.h>

#include "utils/boost.h"
#include "utils/common.h"

namespace xlb::utils {

// A master-slave eventual consistency map for multi-thread, slave is read-only
template <typename K, typename V> class AsyncMap {
public:
  static const uint16_t kMaxThread = 64;
  static const size_t kDefaultOpsLength = 256;

  template <typename... Args>
  explicit AsyncMap(size_t max_threads, Args &&... args)
      : max_threads_(max_threads), num_threads_(), maps_(), queues_() {

    for (auto i : irange(max_threads_)) {
      queues_.emplace_back(
          maps_.emplace_back(std::forward<Args>(args)...).socket(),
          kDefaultOpsLength, false, true);
    }

    ++num_threads_;
  }

  // All unregistered thread are masters
  void RegisterSlave() {
    if (tid_ != 0)
      tid_ = num_threads_.fetch_add(1);

    CHECK_LE(num_threads_, max_threads_);
  }

private:
  using Map = CuckooMap<K, V>;

  struct OpLog {
    std::function<bool(Map *)> func;
    // reference counting
    std::atomic<uint16_t> rc;
  };

  using Queue = LockLessQueue<OpLog *>;

  std::vector<Map> maps_;
  std::vector<Queue> queues_;

  const uint16_t max_threads_;
  std::atomic<uint16_t> num_threads_;

  std::shared_mutex master_rwlock_;

  static __thread uint16_t tid_;

  bool commit(OpLog *op_log) {
    auto ret = op_log->func(&maps_[tid_]);

    if (--op_log->rc == 0)
      delete op_log;

    return ret;
  }

  bool commit_safe(OpLog *op_log) {
    --op_log->rc;
    return op_log->func(&maps_[0]);
  }

  bool fanout(OpLog *op_log) {
    for (auto i : irange((uint16_t)1, max_threads_))
      if (i != tid_)
        while (queues_[i].Push(op_log) == -2)
          ;

    return true;
  }

public:
  void SyncSlave() {
    for (;;) {
      OpLog *op_log{};
      if (queues_[tid_].Pop(op_log) != 0)
        break;

      commit(op_log);
    }
  }

  // TODO: async lock-less api

  bool InsertMaster(const K &key, const V &value) {
    // must call in master
    auto op_log = new OpLog{
        [key, value](Map *map) { return map->Insert(key, value) != nullptr; },
        max_threads_};

    std::unique_lock lock(master_rwlock_);
    return !commit_safe(op_log) || fanout(op_log);
  }

  template <typename... Args> bool EmplaceMaster(const K &key, Args &&... args) {
    auto op_log = new OpLog{[key, args...](Map *map) {
                              return map->Emplace(key, args...) != nullptr;
                            },
                            max_threads_};

    std::unique_lock lock(master_rwlock_);
    return !commit_safe(op_log) || fanout(op_log);
  }

  // Return false if there isn't k in master's
  bool RemoveMaster(const K &key) {
    auto op_log =
        new OpLog{[key](Map *map) { return map->Remove(key); }, max_threads_};

    std::unique_lock lock(master_rwlock_);
    return commit_safe(op_log) || fanout(op_log);
  }

  typename Map::Entry *FindMaster(const K &key) {
    std::shared_lock lock(master_rwlock_);
    return maps_[tid_].Find(key);
  }

  // Must call in slave
  typename Map::Entry *FindSlave(const K &key) { return maps_[tid_].Find(key); }
};

template <typename K, typename V> __thread uint16_t AsyncMap<K, V>::tid_ = {};

}  // namespace xlb::utils
