#include "rpc/control.h"

#include <sstream>

#include <brpc/closure_guard.h>
#include <glog/logging.h>

#include "runtime/config.h"
#include "runtime/exec.h"

#include "headers/ip.h"

#include "conntrack/metric.h"
#include "conntrack/table.h"
#include "conntrack/tuple.h"

#include "utils/boost.h"
#include "utils/channel.h"
#include "utils/common.h"
#include "utils/endian.h"

namespace xlb::rpc {

using conntrack::Tuple2;
using headers::ParseIpv4Address;
using utils::any_of_equal;
using utils::be16_t;
using utils::be32_t;

namespace {

std::pair<bool, Tuple2> validate_service(const Service &svc, Error *err) {
  std::stringstream ss;

  if (any_of_equal(CONFIG.nic.local_ips, svc.addr()) ||
      CONFIG.kni.ip_address == svc.addr()) {
    ss << "forbidden service ip: " << svc.addr();
    goto FAILED;
  }

  if (svc.port() > 65535) {
    ss << "forbidden service port: " << svc.port();
    goto FAILED;
  }

  Tuple2 tuple;

  if (!ParseIpv4Address(svc.addr(), &tuple.ip)) {
    ss << "invalid service ip: " << svc.addr();
    goto FAILED;
  }

  tuple.port = be16_t(svc.port());

  return {true, tuple};

FAILED:
  auto str = ss.str();
  F_LOG(ERROR) << str;
  err->set_code(-1);
  err->set_errmsg(str);
  return {false, {}};
}

}  // namespace

void ControlImpl::AddVirtualService(RpcController *controller,
                                    const VirtualServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InMaster([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);
    auto metric = SMPOOL.GetVs(tuple);

    if (!STABLE.EnsureVsExist(tuple, metric)) {
      response->mutable_error()->set_code(-1);
      response->mutable_error()->set_errmsg(STABLE.LastError());
      SMPOOL.PurgeVs(tuple);
      return;
    }
    Exec::InSlaves([tuple, metric]() { STABLE.EnsureVsExist(tuple, metric); });

    response->mutable_error()->set_code(0);
  });
}

void ControlImpl::DelVirtualService(RpcController *controller,
                                    const VirtualServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InMaster([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);

    STABLE.EnsureVsNotExist(tuple);
    SMPOOL.PurgeVs(tuple);

    Exec::InSlaves([tuple]() { STABLE.EnsureVsNotExist(tuple); });

    response->mutable_error()->set_code(0);
  });
}

void ControlImpl::ListVirtualService(RpcController *controller,
                                     const EmptyRequest *request,
                                     ServicesResponse *response,
                                     Closure *done) {
  brpc::ClosureGuard done_guard(done);

  done_guard.release();

  Exec::InMaster([response, done]() {
    brpc::ClosureGuard done_guard(done);

    for (auto &vs : STABLE) {
      auto resp_vs = response->add_list();
      resp_vs->set_addr(ToIpv4Address(vs.key.ip));
      resp_vs->set_port(vs.key.port.value());
    }

    response->mutable_error()->set_code(0);
  });
}

void ControlImpl::AttachRealService(RpcController *controller,
                                    const RealServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto vpair = validate_service(request->virt(), response->mutable_error());
  if (!vpair.first) return;
  auto rpair = validate_service(request->real(), response->mutable_error());
  if (!rpair.first) return;

  done_guard.release();

  Exec::InMaster(
      [vtuple = vpair.second, rtuple = rpair.second, response, done]() {
        brpc::ClosureGuard done_guard(done);

        auto vs = STABLE.FindVs(vtuple);
        if (!vs) {
          response->mutable_error()->set_code(-1);
          response->mutable_error()->set_errmsg("virtual service is not exist");
          return;
        }

        auto metric = SMPOOL.GetRs(rtuple);
        auto rs = STABLE.EnsureRsExist(rtuple, metric);
        auto rs_fail = [&]() {
          response->mutable_error()->set_code(-1);
          response->mutable_error()->set_errmsg(STABLE.LastError());
          if (STABLE.RsDetached(rtuple)) SMPOOL.PurgeRs(rtuple);
        };

        if (!rs) {
          rs_fail();
          return;
        }

        if (!STABLE.EnsureRsAttachedTo(vs, rs)) {
          rs_fail();
          return;
        }

        Exec::InSlaves([vs, rs, metric]() {
          STABLE.EnsureRsExist(rs->tuple(), metric);
          STABLE.EnsureRsAttachedTo(vs, rs);
        });

        response->mutable_error()->set_code(0);
      });
}

void ControlImpl::DetachRealService(RpcController *controller,
                                    const RealServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto vpair = validate_service(request->virt(), response->mutable_error());
  if (!vpair.first) return;
  auto rpair = validate_service(request->real(), response->mutable_error());
  if (!rpair.first) return;

  done_guard.release();

  Exec::InMaster(
      [vtuple = vpair.second, rtuple = rpair.second, response, done]() {
        brpc::ClosureGuard done_guard(done);

        if (!STABLE.FindVs(vtuple)) {
          response->mutable_error()->set_code(-1);
          response->mutable_error()->set_errmsg("virtual service is not exist");
          return;
        }

        STABLE.EnsureRsDetachedFrom(vtuple, rtuple);
        if (STABLE.RsDetached(rtuple)) SMPOOL.PurgeRs(rtuple);

        Exec::InSlaves([vtuple, rtuple]() {
          STABLE.EnsureRsDetachedFrom(vtuple, rtuple);
        });

        response->mutable_error()->set_code(0);
      });
}

void ControlImpl::ListRealService(RpcController *controller,
                                  const VirtualServiceRequest *request,
                                  ServicesResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InMaster([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);

    auto vs = STABLE.FindVs(tuple);

    if (!vs) {
      response->mutable_error()->set_code(-1);
      response->mutable_error()->set_errmsg("virtual service is not exist");
      return;
    }

    for (auto &rs : *vs) {
      auto resp_rs = response->add_list();
      resp_rs->set_addr(ToIpv4Address(rs->tuple().ip));
      resp_rs->set_port(rs->tuple().port.value());
    }

    response->mutable_error()->set_code(0);
  });
}

}  // namespace xlb::rpc
