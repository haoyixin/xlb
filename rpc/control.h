#pragma once

#include <brpc/server.h>

#include "utils/common.h"

namespace xlb::rpc {

class RpcServer {
 public:
    static void Launch();
    static void Abort();
    static void Wait();

 private:
    static brpc::Server server_;
};


}  // namespace xlb::rpc
