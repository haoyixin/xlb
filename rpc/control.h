#pragma once

#include "rpc/pb/xlb.pb.h"

namespace xlb::rpc {

using google::protobuf::RpcController, google::protobuf::Closure;

class ControlImpl final : public Control {
 public:
  ControlImpl() = default;
  ~ControlImpl() override = default;

  void AddVirtualService(RpcController *controller,
                         const VirtualServiceRequest *request,
                         GeneralResponse *response, Closure *done) override;

  void DelVirtualService(RpcController *controller,
                         const VirtualServiceRequest *request,
                         GeneralResponse *response, Closure *done) override;

  void ListVirtualService(RpcController *controller,
                          const EmptyRequest *request,
                          ServicesResponse *response, Closure *done) override;

  void AttachRealService(RpcController *controller,
                         const RealServiceRequest *request,
                         GeneralResponse *response, Closure *done) override;

  void DetachRealService(RpcController *controller,
                         const RealServiceRequest *request,
                         GeneralResponse *response, Closure *done) override;

  void ListRealService(RpcController *controller,
                       const VirtualServiceRequest *request,
                       ServicesResponse *response, Closure *done) override;
};

}  // namespace xlb::rpc
