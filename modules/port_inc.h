#ifndef XLB_MODULES_PORT_INC_H
#define XLB_MODULES_PORT_INC_H

#include "ports/pmd.h"
#include "module.h"
#include "port.h"
#include "worker.h"

DECLARE_PORT(PMD);

namespace xlb {
namespace modules {

class PortInc final : public Module {

public:
  PortInc(std::string &name, Port *port) : Module(name), port_(port) {
    RegisterTask(&PortInc::RecvTask, nullptr);
  }

  TaskResult RecvTask(Context *ctx, PacketBatch *batch, void *arg) {
    batch->set_cnt(port_->RecvPackets(ctx->worker->current()->id(), batch->pkts(),
                                      Packet::kMaxBurst));
    return {.packets = batch->cnt()};
  }

private:
  Port *port_;
};

} // namespace modules
} // namespace xlb

#endif // XLB_MODULES_PORT_INC_H
