#include "modules/conntrack.h"
#include "conntrack/table.h"

namespace xlb::modules {

void Conntrack::InitInTrivial() {
  STABLE_INIT();
  Exec::RegisterTrivial();

  RegisterTask([](Context *) -> Result { return {.packets = Exec::Sync()}; },
               100);
}

//void Conntrack::InitInMaster() {}

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
