#include "control.h"
#include "config.h"

#include "pb/xlb.pb.h"
#include <brpc/server.h>
#include <glog/logging.h>

namespace xlb {
namespace {

using pb::Control, pb::Value, pb::Error, pb::EmptyRequest, pb::EmptyResponse,
    pb::ConfResponse, pb::GlobalConfRequest, pb::VirtualServerIdent,
    pb::ListVirtualServerResponse, pb::VirtualServerConfRequest,
    pb::RealServerIdent, pb::ListRealServerResponse, pb::RealServerConfRequest;

using google::protobuf::RpcController, google::protobuf::Closure;

class ControlImpl final : public Control {
public:
  ControlImpl() = default;
  ~ControlImpl() override = default;

  void SetGlobalConf(RpcController *controller,
                     const GlobalConfRequest *request, EmptyResponse *response,
                     Closure *done) override {}

  void GetGlobalConf(RpcController *controller, const EmptyRequest *request,
                     ConfResponse *response, Closure *done) override {}

  void AddVirtualServer(RpcController *controller,
                        const VirtualServerIdent *request,
                        EmptyResponse *response, Closure *done) override {}

  void DelVirtualServer(RpcController *controller,
                        const VirtualServerIdent *request,
                        EmptyResponse *response, Closure *done) override {}

  void ListVirtualServer(RpcController *controller, const EmptyRequest *request,
                         ListVirtualServerResponse *response,
                         Closure *done) override {}

  void SetVirtualServerConf(RpcController *controller,
                            const VirtualServerConfRequest *request,
                            EmptyResponse *response, Closure *done) override {}

  void GetVirtualServerConf(RpcController *controller,
                            const VirtualServerIdent *request,
                            ConfResponse *response, Closure *done) override {}

  void AddRealServer(RpcController *controller, const RealServerIdent *request,
                     EmptyResponse *response, Closure *done) override {}

  void DelRealServer(RpcController *controller, const RealServerIdent *request,
                     EmptyResponse *response, Closure *done) override {}

  void ListRealServer(RpcController *controller,
                      const VirtualServerIdent *request,
                      ListRealServerResponse *response,
                      Closure *done) override {}
  void SetRealServerConf(RpcController *controller,
                         const RealServerConfRequest *request,
                         EmptyResponse *response, Closure *done) override {}

  void GetRealServerConf(RpcController *controller,
                         const RealServerIdent *request, ConfResponse *response,
                         Closure *done) override {}
};

} // namespace

void ApiServer::Run() {
  brpc::Server server;
  ControlImpl control_impl;
  CHECK_EQ(server.AddService(&control_impl, brpc::SERVER_OWNS_SERVICE), 0);

  // TODO: ssl options
  brpc::ServerOptions options;
  LOG(ERROR) << CONFIG.rpc_ip_port;
  CHECK_EQ(server.Start(CONFIG.rpc_ip_port.c_str(), &options), 0);

  server.RunUntilAskedToQuit();
}

} // namespace xlb
