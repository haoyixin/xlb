#include "task.h"
#include "module.h"
#include "context.h"

namespace xlb {

Task::ProtoMap *Task::protos_;

Task::Result Task::Run(Context *ctx) const {
  PacketBatch init_batch;
  // Start from the module (task module)
  Result result = module()->RunTask(ctx, &init_batch, arg_);
  deadend(ctx, &dead_batch_);
  return result;
}

} // namespace xlb
