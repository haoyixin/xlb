#pragma once

#include <stack>

#include "utils/x_map.h"
#include "utils/timer.h"

#include "common.h"
#include "conn.h"
#include "metric.h"
#include "service.h"
#include "tuple.h"

namespace xlb {

class SvcTable {
 public:
  SvcTable()
      : vs_map_(CONFIG.svc.max_virtual_service),
        rs_map_(ALLOC),
        rs_vs_map_(ALLOC) {
    rs_map_.reserve(CONFIG.svc.max_virtual_service);
    rs_vs_map_.reserve(CONFIG.svc.max_real_per_virtual *
                       CONFIG.svc.max_virtual_service);

    DLOG_W(INFO) << "Initializing SvcTable succeed";
  }
  ~SvcTable() = default;

  inline VirtSvc::Ptr FindVs(Tuple2 &tuple);

  inline VirtSvc::Ptr EnsureVsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric);
  inline RealSvc::Ptr EnsureRsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric);

  inline bool EnsureRsAttachedTo(VirtSvc::Ptr &vs, RealSvc::Ptr &rs);
  inline void EnsureRsDetachedFrom(Tuple2 &vs, Tuple2 &rs);

  inline void EnsureVsNotExist(Tuple2 &vs);
  // This should only be called in the master to confirm whether the rs-metric
  // in the metric-pool can be purged
  inline bool RsDetached(Tuple2 &rs);

  const std::string &LastError() { return last_error_; }

 private:
  using VsMap = utils::XMap<Tuple2, VirtSvc::Ptr>;
  using RsMap = utils::unordered_map<Tuple2, RealSvc *>;
  using RelatedMap = utils::unordered_multimap<Tuple2, Tuple2>;

  VsMap vs_map_;
  // In order to reuse detached rs, since local-tuple-pool in rs must be unique
  // for the same rs-tuple in the same worker to avoid collision in snat
  RsMap rs_map_;
  RelatedMap rs_vs_map_;

  std::string last_error_;

  friend VirtSvc;
  friend RealSvc;

  DISALLOW_COPY_AND_ASSIGN(SvcTable);
};

#define STABLE utils::UnsafeSingletonTLS<SvcTable>::instance()

class ConnTable {
 public:
  ConnTable()
      : conns_(align_ceil_pow2(CONFIG.svc.max_conn), ALLOC),
        idx_map_(conns_.capacity()),
        idx_pool_(utils::make_vector<idx_t>(conns_.capacity())) {
    // Since 0 means trick of empty
    for (auto idx : irange((size_t)1, conns_.capacity())) idx_pool_.push(idx);
  }
  ~ConnTable() = default;

  inline Conn *Find(Tuple4 &tuple);

  inline Conn *EnsureConnExist(VirtSvc::Ptr &vs_ptr, Tuple2 &cli_tp);

  // Conn will only be destroyed when the timer fires
//  inline bool Destroy(Conn *conn);

 private:
  using IdxMap = utils::XMap<Tuple4, idx_t>;

  utils::vector<Conn> conns_;
  IdxMap idx_map_;
  std::stack<idx_t, utils::vector<idx_t>> idx_pool_;

  utils::TimerWheel<Conn> timer_;

  friend Conn;
  DISALLOW_COPY_AND_ASSIGN(ConnTable);
};

#define CTABLE utils::UnsafeSingletonTLS<ConnTable>::instance()

}  // namespace xlb
