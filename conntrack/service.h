#pragma once

#include "conntrack/common.h"
#include "conntrack/metric.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

class SvcBase : public EventBase<SvcBase>, public INew {
 public:
  explicit SvcBase(const Tuple2 &tuple);
  // WARNING: This is not a virtual function (optimized for size)
  ~SvcBase() = default;

  auto &tuple() const { return tuple_; }

  void IncrConns(uint64_t n) { conns_ += n; }
  void IncrPacketsIn(uint64_t n) { packets_in_ += n; }
  void IncrBytesIn(uint64_t n) { bytes_in_ += n; }
  void IncrPacketsOut(uint64_t n) { packets_out_ += n; }
  void INcrBytesOut(uint64_t n) { bytes_out_ += n; }

  void execute(TimerWheel<SvcBase> *timer);
  auto &metrics() { return metrics_; }
  void set_metrics(const SvcMetrics::Ptr &metric) { metrics_ = metric; }

 protected:
  static const uint64_t kTimerStart = 3000;
  static const uint64_t kTimerEnd = 3100;

  inline void reset_metrics();
  inline void commit_metrics();

  Tuple2 tuple_;
  SvcMetrics::Ptr metrics_;
  //  class SvcTable *stable_;

 private:
  uint64_t conns_;
  uint64_t packets_in_;
  uint64_t bytes_in_;
  uint64_t packets_out_;
  uint64_t bytes_out_;

  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SvcBase);
};

class alignas(64) RealSvc : public unsafe_intrusive_ref_counter<RealSvc>,
                            public SvcBase {
 public:
  using Ptr = intrusive_ptr<RealSvc>;

  ~RealSvc();

  // Return false if empty
  bool GetLocal(Tuple2 &tuple);
  void PutLocal(const Tuple2 &tuple);

 private:
  explicit RealSvc(const Tuple2 &tuple) : SvcBase(tuple), local_tuple_pool_() {
    W_DVLOG(1) << "creating: " << tuple;
    bind_local_ips();
  }

  void bind_local_ips();

  std::stack<Tuple2, vector<Tuple2>> local_tuple_pool_;

  friend class VirtSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RealSvc);
};

static_assert(sizeof(RealSvc) == 128);

class alignas(64) VirtSvc : public unsafe_intrusive_ref_counter<VirtSvc>,
                            public SvcBase {
 public:
  using Ptr = intrusive_ptr<VirtSvc>;
  using RsVec = vector<RealSvc::Ptr>;

  ~VirtSvc() = default;

  inline RealSvc::Ptr SelectRs(const Tuple2 &ctuple);

  // TODO: lazy transform
  //  auto begin() { return rs_vec_.begin(); }
  //  auto end() { return rs_vec_.end(); }

 private:
  explicit VirtSvc(const Tuple2 &tuple) : SvcBase(tuple), rs_vec_(ALLOC) {
    W_DVLOG(1) << "creating: " << tuple;
    //    rs_vec_.reserve(CONFIG.svc.max_real_per_virtual);
  }

  RsVec rs_vec_;

  friend RealSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VirtSvc);
};

static_assert(sizeof(VirtSvc) == 128);

}  // namespace xlb::conntrack
