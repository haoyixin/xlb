#include "rpc/control.h"

#include <sstream>

#include <brpc/closure_guard.h>
#include <bthread/bthread.h>
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

using conntrack::SvcMetrics;
using conntrack::Tuple2;
using headers::ParseIpv4Address;
using utils::any_of_equal;
using utils::be16_t;
using utils::be32_t;

namespace {

template <typename T>
void make_error(T *resp, const std::string &str) {
  resp->mutable_error()->set_code(-1);
  resp->mutable_error()->set_errmsg(str);
}

template <typename T>
void make_warn(T *resp, const std::string &str) {
  resp->mutable_error()->set_code(1);
  resp->mutable_error()->set_errmsg(str);
}

template <typename T>
void make_ok(T *resp) {
  resp->mutable_error()->set_code(0);
  resp->mutable_error()->set_errmsg("");
}

std::pair<bool, Tuple2> validate_service(const Service &svc, Error *err) {
  std::stringstream ss;

  if (any_of_equal(CONFIG.nic.local_ips, svc.addr()) ||
      CONFIG.kni.ip_address == svc.addr()) {
    ss << "forbidden service ip: " << svc.addr();
    goto FAILED;
  }

  if (svc.port() > 65535) {
    ss << "invalid service port: " << svc.port();
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

void trivial_done(Closure *done) {
  Exec::InTrivial([done]() { brpc::ClosureGuard done_guard(done); });
}

}  // namespace

void ControlImpl::AddVirtualService(RpcController *controller,
                                    const VirtualServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InTrivial([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);

    if (STABLE.FindRs(tuple)) {
      make_error(response, "recursion is not allowed");
      return;
    }

    auto vs = STABLE.FindVs(tuple);
    if (vs) {
      make_warn(response, "virtual service already exists");
      return;
    }

    if (STABLE.CountVs() >= CONFIG.svc.max_virtual_service) {
      make_error(response, "number of virtual service exceeds the limit");
      return;
    }

    vs = STABLE.AddVs(tuple);

    if (!vs) {
      make_error(response, STABLE.LastError());
      return;
    }

    auto metric = SvcMetrics::Get();
    // Just in case
    if (!metric->Expose("virt", tuple)) {
      STABLE.RemoveVs(vs);
      make_error(response, "failed to expose metrics");
      return;
    }
    vs->set_metrics(metric);

    Exec::InSlaves(
        [tuple, metric]() { STABLE.AddVs(tuple)->set_metrics(metric); });

    make_ok(response);
    done_guard.release();
    trivial_done(done);
  });
}

void ControlImpl::DelVirtualService(RpcController *controller,
                                    const VirtualServiceRequest *request,
                                    GeneralResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InTrivial([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);

    auto vs = STABLE.FindVs(tuple);
    if (!vs) {
      make_warn(response, "virtual service does not exist");
      return;
    }

    // vs->metrics()->Hide();
    // STABLE.ForeachRs(vs, [](auto *rs) { rs->metrics()->Hide(); });
    STABLE.RemoveVs(vs);

    Exec::InSlaves([tuple]() { STABLE.RemoveVs(STABLE.FindVs(tuple)); });

    make_ok(response);
    done_guard.release();
    trivial_done(done);
  });
}

void ControlImpl::ListVirtualService(RpcController *controller,
                                     const EmptyRequest *request,
                                     ServicesResponse *response,
                                     Closure *done) {
  brpc::ClosureGuard done_guard(done);

  done_guard.release();

  Exec::InTrivial([response, done]() {
    brpc::ClosureGuard done_guard(done);

    STABLE.ForeachVs([response](auto *vs) {
      auto *resp_vs = response->add_list();
      resp_vs->set_addr(ToIpv4Address(vs->tuple().ip));
      resp_vs->set_port(vs->tuple().port.value());
    });

    if (response->list().empty()) {
      make_warn(response, "there is no virtual service");
      return;
    }

    make_ok(response);
    done_guard.release();
    trivial_done(done);
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

  if (vpair.second == rpair.second) {
    make_error(response, "real service and virtual service can not be same");
    return;
  }

  done_guard.release();

  Exec::InTrivial(
      [vtuple = vpair.second, rtuple = rpair.second, response, done]() {
        brpc::ClosureGuard done_guard(done);

        if (STABLE.FindVs(rtuple)) {
          make_error(response, "recursion is not allowed");
          return;
        }

        auto vs = STABLE.FindVs(vtuple);
        if (!vs) {
          make_error(response, "virtual service does not exist");
          return;
        }

        auto rs = STABLE.FindRs(rtuple);
        if (rs) {
          // In the master, the existence of rs means that there must be a vs
          // reference to it
          auto pair = STABLE.RsAttached(vs, rs);
          if (pair.first) {
            make_warn(response, "real service has attached");
            return;
          }
        } else {
          if (STABLE.CountRs() >= CONFIG.svc.max_real_service) {
            make_error(response, "number of real service exceeds the limit");
            return;
          }
          // If the 'AttachRs' is not called, rs will destroy at the end of the
          // closure
          rs = STABLE.AddRs(rtuple);
          auto metric = SvcMetrics::Get();
          // Just in case
          if (!metric->Expose("real", rtuple)) {
            make_error(response, "failed to expose metrics");
            return;
          }

          rs->set_metrics(metric);
        }

        if (STABLE.CountRs(vs) >= CONFIG.svc.max_real_per_virtual) {
          make_error(
              response,
              "number of real service per virtual service exceeds the limit");
          return;
        }

        STABLE.AttachRs(vs, rs);

        Exec::InSlaves([vtuple, rtuple, metric = rs->metrics()]() {
          STABLE.AttachRs(STABLE.FindVs(vtuple), STABLE.AddRs(rtuple))
              ->set_metrics(metric);
        });

        make_ok(response);
        done_guard.release();
        trivial_done(done);
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

  Exec::InTrivial(
      [vtuple = vpair.second, rtuple = rpair.second, response, done]() {
        brpc::ClosureGuard done_guard(done);

        auto vs = STABLE.FindVs(vtuple);
        if (!vs) {
          make_warn(response, "virtual service does not exist");
          return;
        }

        auto rs = STABLE.FindRs(rtuple);
        if (rs) {
          auto pair = STABLE.RsAttached(vs, rs);
          if (!pair.first) {
            make_warn(response, "real service has detached");
            return;
          }

          STABLE.DetachRs(vs, rs, pair.second);

          // For the case of adding the same rs immediately after deletion
          // if (STABLE.RsDetached(rs)) rs->metrics()->Hide();

          Exec::InSlaves([vtuple, rtuple]() {
            auto vs = STABLE.FindVs(vtuple);
            auto rs = STABLE.FindRs(rtuple);

            STABLE.DetachRs(vs, rs, STABLE.RsAttached(vs, rs).second);
          });
        } else {
          make_warn(response, "real service does not exist");
          return;
        }

        make_ok(response);
        done_guard.release();
        trivial_done(done);
      });
}

void ControlImpl::ListRealService(RpcController *controller,
                                  const VirtualServiceRequest *request,
                                  ServicesResponse *response, Closure *done) {
  brpc::ClosureGuard done_guard(done);

  auto pair = validate_service(request->svc(), response->mutable_error());
  if (!pair.first) return;

  done_guard.release();

  Exec::InTrivial([tuple = pair.second, response, done]() {
    brpc::ClosureGuard done_guard(done);

    auto vs = STABLE.FindVs(tuple);

    if (!vs) {
      make_error(response, "virtual service does not exist");
      return;
    }

    STABLE.ForeachRs(vs, [response](auto *rs) {
      auto resp_rs = response->add_list();
      resp_rs->set_addr(ToIpv4Address(rs->tuple().ip));
      resp_rs->set_port(rs->tuple().port.value());
    });

    if (response->list().empty()) {
      make_warn(response, "there is no real service");
      return;
    }

    make_ok(response);
    done_guard.release();
    trivial_done(done);
  });
}

}  // namespace xlb::rpc
