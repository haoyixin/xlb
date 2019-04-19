#pragma once

#include "conntrack/common.h"
#include "conntrack/conn.h"
#include "conntrack/metric.h"
#include "conntrack/service.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

class SvcTable {
 public:
  SvcTable()
      : vs_map_(CONFIG.svc.max_virtual_service),
        rs_map_(ALLOC),
        rs_vs_map_(ALLOC),
        timer_(W_TSC) {
    rs_map_.reserve(CONFIG.svc.max_virtual_service);
    rs_vs_map_.reserve(CONFIG.svc.max_real_per_virtual *
                       CONFIG.svc.max_virtual_service);

    W_LOG(INFO) << "initializing succeed";
  }
  ~SvcTable() = default;

  inline VirtSvc::Ptr FindVs(Tuple2 &tuple);

  // TODO: make sure local ip is not used as vip or rsip
  inline VirtSvc::Ptr EnsureVsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric);
  inline RealSvc::Ptr EnsureRsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric);

  inline bool EnsureRsAttachedTo(VirtSvc::Ptr &vs, RealSvc::Ptr &rs);
  inline void EnsureRsDetachedFrom(Tuple2 &vs, Tuple2 &rs);

  inline void EnsureVsNotExist(Tuple2 &vs);
  // This should only be called in the master to confirm whether the rs-metric
  // in the metric-pool can be purged
  inline bool RsDetached(Tuple2 &rs);

  const std::string &LastError() { return last_error_; }

  size_t Sync() { return timer_.AdvanceTo(W_TSC); }

 private:
  using VsMap = XMap<Tuple2, VirtSvc::Ptr>;
  using RsMap = unordered_map<Tuple2, RealSvc *>;
  using RelatedMap = unordered_multimap<Tuple2, Tuple2>;

  VsMap vs_map_;
  // In order to reuse detached rs, since local-tuple-pool in rs must be unique
  // for the same rs-tuple in the same worker to avoid collision in snat
  RsMap rs_map_;
  RelatedMap rs_vs_map_;

  TimerWheel<SvcBase> timer_;

  std::string last_error_;

  friend VirtSvc;
  friend RealSvc;
  friend SvcBase;

  DISALLOW_COPY_AND_ASSIGN(SvcTable);
};

class ConnTable {
 public:
  ConnTable()
      : conns_(align_ceil_pow2(CONFIG.svc.max_conn), ALLOC),
        idx_map_(conns_.capacity()),
        idx_pool_(make_vector<uint32_t>(conns_.capacity())),
        timer_(W_TSC) {
    // Since 0 means trick of empty
    for (auto idx : irange(1ul, conns_.capacity())) idx_pool_.push(idx);
    W_LOG(INFO) << "initializing succeed";
  }
  ~ConnTable() = default;

  inline Conn *Find(Tuple4 &tuple);
  inline Conn *EnsureConnExist(VirtSvc::Ptr &vs_ptr, Tuple2 &cli_tp);

  size_t Sync() { return timer_.AdvanceTo(W_TSC); }

 private:
  using IdxMap = XMap<Tuple4, uint32_t>;

  vector<Conn> conns_;
  IdxMap idx_map_;
  std::stack<uint32_t, vector<uint32_t>> idx_pool_;

  TimerWheel<Conn> timer_;

  friend Conn;
  DISALLOW_COPY_AND_ASSIGN(ConnTable);
};

}  // namespace xlb::conntrack

namespace xlb {

#define STABLE_INIT (UnsafeSingletonTLS<conntrack::SvcTable>::Init)
#define STABLE (UnsafeSingletonTLS<conntrack::SvcTable>::instance())

#define CTABLE_INIT (UnsafeSingletonTLS<conntrack::ConnTable>::Init)
#define CTABLE (UnsafeSingletonTLS<conntrack::ConnTable>::instance())

}
