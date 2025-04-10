#pragma once

#include "modules/common.h"

namespace xlb::modules {

class TcpInc : public Module {
 public:
  void InitInTrivial() override;
  void InitInSlave(uint16_t) override;

  template <typename Tag = NoneTag>
  inline void Process(Context *ctx, Packet *packet);
};

}  // namespace xlb::modules
