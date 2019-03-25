#pragma once

#include "modules/common.h"

namespace xlb::modules {

class TcpInc : public Module {
 public:
  template <typename Tag = NoneTag>
  void Process(Context *ctx, Packet *packet);
};

}  // namespace xlb::modules
