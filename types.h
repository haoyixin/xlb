#ifndef XLB_COMMON_H
#define XLB_COMMON_H

#include <vector>

namespace xlb {

class Module;
class Task;
class Worker;
class PacketBatch;

struct Context {
  Task *task;
  Worker *worker;
  uint64_t silent_drops;
};

struct TaskResult {
  uint64_t packets;
};

template <typename T>
using ModuleFunc = TaskResult (T::*)(Context *, PacketBatch *, void *);

using TaskFunc = std::function<TaskResult(Context *, PacketBatch *)>;

using TaskFuncs = std::vector<TaskFunc>;

} // namespace xlb

#endif // XLB_COMMON_H
