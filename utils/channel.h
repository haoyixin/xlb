#pragma once

#include "utils/boost.h"
#include "utils/common.h"
#include "utils/lock_less_queue.h"

namespace xlb::utils {

template <typename T, typename Tag = DefaultTag>
class Channel {
 public:
  using Ptr = std::shared_ptr<T>;

  Channel() = delete;
  ~Channel() = delete;

  // 'G' means group
  template <typename G>
  static void Register(size_t size) {
    // This is safe since 'Init' will judge whether it is null
    auto *ring = &UnsafeSingletonTLS<Ring>::Init(size);

    std::lock_guard guard(internal<G>::lock);
    if (any_of_equal(internal<G>::rings, ring)) return;

    internal<G>::rings.emplace_back(ring);
  }

  // TODO: unregister...

  template <typename G>
  static void Send(Ptr sp) {
    for (auto &r : internal<G>::rings) {
      auto *spp = new Ptr(sp);
      for (;;) {
        if (r->Push(spp))
          break;
        else
          LOG(WARNING) << "channel is full, retrying";
      }
    }
  }

  //  template <typename G>
  //  static void Send(T *ptr) {
  //    send(ptr);
  //  }
  //
  //  template <typename G>
  //  static void Send(Ptr &&sp) {
  //    send(sp);
  //  }
  //
  //  template <typename G>
  //  static void Send(Ptr &sp) {
  //    send(sp);
  //  }

  template <typename G, typename... Args>
  static void Send(Args &&... args) {
    Send<G>(make_shared<T>(std::forward<Args>(args)...));
  }

  static Ptr Recv() {
    Ptr *spp;
    // Calling 'Recv' in unregistered thread is undefined behavior
    auto &ring = UnsafeSingletonTLS<Ring>::instance();
    if (!ring.Pop(spp)) return {};

    Ptr sp = *spp;
    delete spp;

    return sp;
  }

 private:
  // Since calling 'Recv' will only read the thread local ring
  using Ring = LockLessQueue<Ptr *, false, true>;

  template <typename G>
  struct internal {
    static vector<Ring *> rings;
    static std::mutex lock;
  };

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

template <typename T, typename Tag>
template <typename G>
vector<typename Channel<T, Tag>::Ring *> Channel<T, Tag>::internal<G>::rings =
    {};

template <typename T, typename Tag>
template <typename G>
std::mutex Channel<T, Tag>::internal<G>::lock = {};

}  // namespace xlb::utils
