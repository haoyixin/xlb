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
using ModuleTaskFunc = TaskResult (T::*)(Context *, PacketBatch *, void *);

template <typename T> using ModuleInitWorkerFunc = void (T::*)(uint16_t);

using TaskFunc = std::function<TaskResult(Context *, PacketBatch *)>;

using InitWorkerFunc = std::function<void(uint16_t)>;

using TaskFuncs = std::vector<TaskFunc>;

using InitWorkerFuncs = std::vector<InitWorkerFunc>;

} // namespace xlb

#endif // XLB_COMMON_H
