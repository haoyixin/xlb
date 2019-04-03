#pragma once

#include "modules/common.h"
#include "table.h"

namespace xlb {

#define VTABLE utils::UnsafeSingletonTLS<VTable>::instance()

}

namespace xlb::modules {

class DNat : public Module {
 public:
    void InitInMaster() override {
      utils::UnsafeSingletonTLS<SvcTable>::Init();
    }

    void InitInSlave(uint16_t wid) override {
      utils::UnsafeSingletonTLS<SvcTable>::Init();
    }


 private:

};

}

