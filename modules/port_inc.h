#ifndef XLB_MODULES_PORT_INC_H
#define XLB_MODULES_PORT_INC_H

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
  PortInc() {
    port_ = xlb::utils::UnsafeSingleton<T>::Set();
    RegisterTask(&PortInc::RecvTask, nullptr);
  }

  TaskResult RecvTask(Context *ctx, PacketBatch *batch, void *arg) {
    batch->set_cnt(port_->RecvPackets(ctx->worker->current()->id(),
                                      batch->pkts(), Packet::kMaxBurst));
    return {.packets = batch->cnt()};
  }

private:
  T *port_;
};

} // namespace modules
} // namespace xlb

#endif // XLB_MODULES_PORT_INC_H
