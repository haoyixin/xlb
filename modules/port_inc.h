#ifndef XLB_MODULES_PORT_INC_H
#define XLB_MODULES_PORT_INC_H

#include "module.h"
#include "port.h"
#include "ports/pmd.h"
#include "worker.h"

DECLARE_PORT(PMD);

namespace xlb {
namespace modules {

class PortInc final : Module {

public:
  PortInc(std::string &name, Port *port) : Module(name), port_(port) {
    RegisterTask(nullptr);
  }

  Task::Result RunTask(Context *ctx, PacketBatch *batch, void *arg) override {
    batch->set_cnt(port_->RecvPackets(Worker::current()->id(), batch->pkts(),
                                      PacketBatch::kMaxBurst));
    return {.packets = batch->cnt()};
  }

private:
  Port *port_;
};

} // namespace modules
} // namespace xlb

#endif // XLB_MODULES_PORT_INC_H
