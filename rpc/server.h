#pragma once

#include <brpc/server.h>

namespace xlb::rpc {

class Server {
 public:
  static void Launch();
  static void Abort();
  static void Wait();

 private:
  static brpc::Server server_;
};

}  // namespace xlb::rpc
