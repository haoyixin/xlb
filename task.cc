#include "task.h"
#include "module.h"

namespace xlb {

utils::CuckooMap<std::string, TaskProto> *Task::task_protos_;

struct task_result Task::operator()(Context *ctx) const {
  PacketBatch init_batch;
  ClearPacketBatch();

  // Start from the module (task module)
  struct task_result result = module_->RunTask(ctx, &init_batch, arg_);

  deadend(ctx, &dead_batch_);

  return result;
}

utils::CuckooMap<std::string, TaskProto> *Task::TaskProtos() {
  if (!task_protos_)
    task_protos_ = new utils::CuckooMap<std::string, TaskProto>();

  return task_protos_;
}

} // namespace xlb
