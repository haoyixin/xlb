#include "module.h"

#include <glog/logging.h>

#include "task.h"

namespace xlb {

struct task_result Module::RunTask(Context *, PacketBatch *, void *) {
  CHECK(0); // You must override this function
  return task_result();
}

void Module::ProcessBatch(Context *, PacketBatch *) {
  CHECK(0); // You must override this function
}

inline void Module::DropPacket(Context *ctx, Packet *pkt) {
  ctx->task->dead_batch()->add(pkt);
  if (static_cast<size_t>(ctx->task->dead_batch()->cnt()) >=
      PacketBatch::kMaxBurst) {
    deadend(ctx, ctx->task->dead_batch());
  }
}

void Module::RegisterTask(void *arg) {
  if (!registered_) {
    Task::TaskProtos()->Emplace(name_, this, arg);
  }
}

Module::~Module() {
  if (registered_)
    Task::TaskProtos()->Remove(name_);
}

} // namespace xlb
