#ifndef XLB_MODULES_PORT_INC_H
#define XLB_MODULES_PORT_INC_H

#include <bvar/bvar.h>

#include "module.h"
#include "port.h"
#include "ports/pmd.h"
#include "worker.h"

// DECLARE_PORT(PMD);

namespace xlb {
namespace modules {


template <typename T> class PortInc final : public Module {
  static_assert(std::is_base_of<Port, T>::value);

public:
  template <typename... Args> explicit PortInc(Args &&... args) {
    port_ = xlb::utils::UnsafeSingleton<T>::Init(std::forward<Args>(args)...);
    RegisterTask(&PortInc::RecvTask, nullptr);
  }

  TaskResult RecvTask(Context *ctx, PacketBatch *batch, void *arg) {
    batch->set_cnt(port_->Recv(ctx->worker->current()->id(),
                                      batch->pkts(), Packet::kMaxBurst));
    return {.packets = batch->cnt()};
  }

private:
  T *port_;
};

} // namespace modules
} // namespace xlb

#endif // XLB_MODULES_PORT_INC_H
