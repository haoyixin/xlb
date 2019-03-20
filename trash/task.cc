#include "trash/task.h"

namespace xlb {

template <typename... Args>
Task::Task(uint8_t weight, Args &&... args)
    : max_weight_(weight), current_weight_(weight) {
  dead_batch_.Clear();
  stage_batch_.Clear();
//  Tasks::instance().emplace_back(this);
}

void Task::DropPacket(Packet *pkt) {
  if (dead_batch_.Full()) dead_batch_.Free();

  dead_batch_.Push(pkt);
}

void Task::HoldPacket(Packet *pkt) {
  if (!stage_batch_.Full())
    stage_batch_.Push(pkt);
  else
    DropPacket(pkt);
}

}  // namespace xlb
