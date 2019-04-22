#include "rpc/server.h"

#include <glog/logging.h>

#include "rpc/control.h"
#include "runtime/config.h"

namespace xlb::rpc {

brpc::Server Server::server_ = {};

void Server::Launch() {
  ControlImpl control_impl;
  CHECK_EQ(server_.AddService(&control_impl, brpc::SERVER_DOESNT_OWN_SERVICE),
           0);

  // TODO: ssl options
  brpc::ServerOptions options;
  CHECK_EQ(server_.Start(CONFIG.rpc_ip_port.c_str(), &options), 0);

  LOG(INFO) << "rpc server started on: " << CONFIG.rpc_ip_port;
}

void Server::Abort() { server_.Stop(0); }

void Server::Wait() { server_.Join(); }

}  // namespace xlb::rpc
