#include "modules/conntrack.h"
#include "conntrack/table.h"

namespace xlb::modules {

void Conntrack::InitInMaster() {
  STABLE_INIT();
  Exec::RegisterMaster();

  RegisterTask([](Context *) -> Result { return {.packets = Exec::Sync()}; },
               10);
}

void Conntrack::InitInSlave(uint16_t) {
  STABLE_INIT();
  CTABLE_INIT();
  Exec::RegisterSlave();

  RegisterTask([](Context *) -> Result { return {.packets = STABLE.Sync()}; },
               1);

  RegisterTask([](Context *) -> Result { return {.packets = Exec::Sync()}; },
               10);

  RegisterTask([](Context *) -> Result { return {.packets = CTABLE.Sync()}; },
               100);
}

template <>
void Conntrack::Process<PMD>(Context *ctx, Packet *packet) {}

}  // namespace xlb::modules
