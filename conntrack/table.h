#pragma once

#include "conntrack/common.h"
#include "conntrack/conn.h"
#include "conntrack/metric.h"
#include "conntrack/service.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

// This is not a must, it is just an insurance, and it is not strict. It is
// purely for the purpose of changing the network segment without restarting the
// service (in fact, in either case, it rarely happens)
/*
class AddrPool {
 public:
  AddrPool() : vs_set_(ALLOC), rs_set_(ALLOC) {
    vs_set_.reserve(CONFIG.svc.max_virtual_service);
    rs_set_.reserve(CONFIG.svc.max_real_service);

    W_LOG(INFO) << "initializing succeed";
  }
  ~AddrPool() = default;

  bool GetVs(const be32_t &addr) {
    if (rs_set_.find(addr) == rs_set_.end()) {
      vs_set_.emplace(addr);
      return true;
    } else {
      return false;
    }
  }
  bool GetRs(const be32_t &addr) {
    if (vs_set_.find(addr) == vs_set_.end()) {
      rs_set_.emplace(addr);
      return true;
    } else {
      return false;
    }
  }

  void PutVs(const be32_t &addr) {
    auto it = vs_set_.find(addr);
    if (it != vs_set_.end()) vs_set_.erase(it);
  }
  void PutRs(const be32_t &addr) {
    auto it = rs_set_.find(addr);
    if (it != rs_set_.end()) rs_set_.erase(it);
  }

 private:
  using VsSet = unordered_multiset<be32_t>;
  using RsSet = unordered_multiset<be32_t>;

  VsSet vs_set_;
  RsSet rs_set_;
};
 */

class SvcTable {
 private:
  using TPool = std::stack<Tuple2, vector<Tuple2>>;
  using VsMap = XMap<Tuple2, VirtSvc::Ptr>;
  using RsMap = unordered_map<Tuple2, RealSvc *>;
  using RelatedMap = unordered_multimap<Tuple2, Tuple2>;
  using Hint = RelatedMap::iterator;

 public:
  SvcTable()
      : vs_map_(CONFIG.svc.max_virtual_service),
        rs_map_(ALLOC),
        rs_vs_map_(ALLOC),
        timer_(W_TSC) {
    rs_map_.reserve(CONFIG.svc.max_real_service);
    rs_vs_map_.reserve(CONFIG.svc.max_real_per_virtual *
                       CONFIG.svc.max_virtual_service);

    if (!W_SLAVE) return;

    auto range = CONFIG.slave_local_ips.equal_range(W_ID);

    for (auto it = range.first; it != range.second; ++it) {
      for (auto i :
           irange((uint16_t)1024u, std::numeric_limits<uint16_t>::max()))
        prototype_.emplace(it->second, be16_t(i));
    }

    W_LOG(INFO) << "initializing succeed";
  }
  ~SvcTable() = default;

  // TODO: more intuitive interface
  VirtSvc::Ptr FindVs(const Tuple2 &tuple);
  RealSvc::Ptr FindRs(const Tuple2 &tuple);

  // WARING: make sure vs does not exists
  VirtSvc::Ptr AddVs(const Tuple2 &tuple);
  void RemoveVs(VirtSvc::Ptr vs);

  RealSvc::Ptr AddRs(const Tuple2 &tuple);
  // WARING: make sure rs is detached
  RealSvc::Ptr AttachRs(VirtSvc::Ptr vs, RealSvc::Ptr rs);
  // This should only be called in the master to confirm whether the rs-metric
  // in the metric-pool can be purged
  // bool RsDetached(RealSvc::Ptr rs);

  std::pair<bool, Hint> RsAttached(VirtSvc::Ptr vs, RealSvc::Ptr rs);
  void DetachRs(VirtSvc::Ptr vs, RealSvc::Ptr rs, Hint it);

  const std::string &LastError() { return last_error_; }

  size_t Sync() { return timer_.AdvanceTo(W_TSC); }

  auto CountRs() { return rs_vs_map_.size(); }
  auto CountRs(VirtSvc::Ptr vs) { return vs->rs_vec_.size(); }
  auto CountVs() { return vs_map_.Size(); }

  template <typename T>
  void ForeachRs(VirtSvc::Ptr vs, T &&func) {
    for_each(vs->rs_vec_, [&func](auto &rs) { func(rs.get()); });
  }
  template <typename T>
  void ForeachRs(T &&func) {
    for_each(rs_map_, [&func](auto &entry) { func(entry->second); });
  }
  template <typename T>
  void ForeachVs(T &&func) {
    std::for_each(vs_map_.begin(), vs_map_.end(),
                  [&func](auto &entry) { func(entry.value.get()); });
  }

  TPool &Prototype() { return prototype_; }

 private:
  TPool prototype_;
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
  inline Conn *EnsureConnExist(VirtSvc::Ptr vs_ptr, const Tuple2 &cli_tp);

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

/*
#define APOOL_INIT (UnsafeSingleton<conntrack::AddrPool>::Init)
#define APOOL (UnsafeSingleton<conntrack::AddrPool>::instance())
 */

#define STABLE_INIT (UnsafeSingletonTLS<conntrack::SvcTable>::Init)
#define STABLE (UnsafeSingletonTLS<conntrack::SvcTable>::instance())

#define CTABLE_INIT (UnsafeSingletonTLS<conntrack::ConnTable>::Init)
#define CTABLE (UnsafeSingletonTLS<conntrack::ConnTable>::instance())

}  // namespace xlb
