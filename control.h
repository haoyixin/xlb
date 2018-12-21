#ifndef XLB_CTRL_H
#define XLB_CTRL_H

#include "config.h"
#include <brpc/server.h>
#include <string>
#include <utils/cuckoo_map.h>
#include <utils/rcu.h>

namespace xlb {

class ApiServer {
public:
  ApiServer() = default;

  // This class is neither copyable nor movable.
  ApiServer(const ApiServer &) = delete;
  ApiServer &operator=(const ApiServer &) = delete;

  // Runs the API server.
  static void Run();
};

template <typename T, typename R> class RpcTask {
public:
  using OpFunc = typename utils::RCU<T>::OpFunc;
  using UpdateFunc = std::function<void(T *, R *)>;
  using ResultMap = typename utils::CuckooMap<int, R>;
  using DoneFunc = std::function<void(ResultMap *)>;
  using ErrorFunc = std::function<void(ResultMap *)>;
  using VerifyFunc = std::function<bool(R *)>;

  RpcTask(utils::RCU<T> *rcu, UpdateFunc update_func, OpFunc rollback_func,
          VerifyFunc verify_func, DoneFunc done_func, ErrorFunc error_func,
          google::protobuf::Closure *done)
      : rcu_(rcu), update_func_(update_func), rollback_func_(rollback_func),
        verify_func_(verify_func), done_func_(done_func),
        error_func_(error_func), done_(done), result_map_(rcu_->Map()) {}

  ~RpcTask() = default;

  void operator()(T *data) {
    update_func_(data, &result_map_->Find(utils::RCU<T>::Tid())->second);

    if (complete_.fetch_add(1) == rcu_->NumThreads() + 1) {
      auto done = done_;

      if (valid()) {
        done_func_(&result_map_);
      } else {
        if (!rollback_func_)
          rcu_->Update(rollback_func_);

        error_func_(&result_map_);
      }

      delete this;
      done->Run();
    }
  }

private:
  bool valid() {
    for (auto &[_, res] : result_map_)
      if (!verify_func_(&res))
        return false;

    return true;
  }

  utils::RCU<T> *rcu_;

  UpdateFunc update_func_;
  OpFunc rollback_func_;
  DoneFunc done_func_;
  VerifyFunc verify_func_;
  ErrorFunc error_func_;

  ResultMap result_map_;
  google::protobuf::Closure *done_;

  std::atomic<int> complete_;
};

} // namespace xlb

#endif // XLB_CTRL_H
