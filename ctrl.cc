#include "ctrl.h"

#include <thread>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <shared_mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "pb/xlb.grpc.pb.h"
#include "utils/format.h"

#pragma GCC diagnostic pop

using grpc::ServerContext;
using grpc::Status;

using namespace xlb::pb;

template <typename T>
static inline Status return_with_error(T *response, int code, const char *fmt,
                                       ...) {
  va_list ap;
  va_start(ap, fmt);
  response->mutable_error()->set_code(code);
  response->mutable_error()->set_errmsg(xlb::utils::FormatVarg(fmt, ap));
  va_end(ap);
  return Status::OK;
}

template <typename T>
static inline Status return_with_errno(T *response, int code) {
  response->mutable_error()->set_code(code);
  response->mutable_error()->set_errmsg(strerror(code));
  return Status::OK;
}

class ControlImpl final : public Control::Service {
public:
  Status AddVirtualServer(ServerContext *, const VirtualServerIdent *ident,
                          EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status DelVirtualServer(ServerContext *, const VirtualServerIdent *ident,
                          EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status ListVirtualServer(ServerContext *, const EmptyRequest *,
                           ListVirtualServerResponse *response) override {}

  Status AddRealServer(ServerContext *, const RealServerIdent *ident,
                       EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status DelRealServer(ServerContext *, const RealServerIdent *ident,
                       EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status ListRealServer(ServerContext *, const VirtualServerIdent *ident,
                        ListRealServerResponse *response) override {}

private:
  // gRPC service handlers are not thread-safe; we serialize them with a lock.
  std::shared_mutex mutex_;
};

class ConfigImpl final : public Config::Service {
public:
  Status SetGlobalConf(ServerContext *, const SetGlobalConfRequest *request,
                       EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status GetGlobalConf(ServerContext *, const EmptyRequest *,
                       ConfResponse *response) override {}

  Status SetVirtualServerConf(ServerContext *,
                              const SetVirtualServerConfRequest *request,
                              EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status GetVirtualServerConf(ServerContext *, const VirtualServerIdent *ident,
                              ConfResponse *response) override {}

  Status SetRealServerConf(ServerContext *,
                           const SetRealServerConfRequest *request,
                           EmptyResponse *response) override {
    return return_with_error(response, -1, "not impl");
  }

  Status GetRealServerConf(ServerContext *, const RealServerIdent *ident,
                           ConfResponse *response) override {}

private:
  std::shared_mutex mutex_;
};

class StatsImpl final : public Stats::Service {
public:
  Status GetGlobalStats(ServerContext *, const EmptyRequest *,
                        StatsResponse *response) override {}

  Status GetVirtualServerStats(ServerContext *, const VirtualServerIdent *ident,
                               StatsResponse *response) override {}

  Status GetRealServerStats(ServerContext *, const RealServerIdent *ident,
                            StatsResponse *response) override {}
};

void ApiServer::Listen(const std::string &addr) {
  if (!builder_) {
    builder_ = new grpc::ServerBuilder();
  }

  LOG(INFO) << "Server listening on " << addr;

  builder_->AddListeningPort(addr, grpc::InsecureServerCredentials());
}

void ApiServer::Run() {
  if (!builder_) {
    // We are not listening on any sockets. There is nothing to do.
    return;
  }

  ControlImpl controlService;
  ConfigImpl configService;
  StatsImpl statsService;

  builder_->RegisterService(&controlService);
  builder_->RegisterService(&configService);
  builder_->RegisterService(&statsService);

  builder_->SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, 1);

  std::unique_ptr<grpc::Server> server = builder_->BuildAndStart();
  if (server == nullptr) {
    LOG(ERROR) << "ServerBuilder::BuildAndStart() failed";
    return;
  }

  server->Wait();
}
