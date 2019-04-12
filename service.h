#pragma once

#include "utils/allocator.h"
#include "utils/x_map.h"

#include "common.h"
#include "config.h"
#include "metric.h"
#include "tuple.h"

namespace xlb {

class SvcBase : public unsafe_intrusive_ref_counter<SvcBase>,
                public EventBase<SvcBase>,
                public INew {
 public:
  SvcBase(Tuple2 &tuple, SvcMetrics::Ptr &metric);
  ~SvcBase() override = default;

  auto &tuple() const { return tuple_; }

  void IncrConns(uint64_t n) { conns_ += n; }
  void IncrPacketsIn(uint64_t n) { packets_in_ += n; }
  void IncrBytesIn(uint64_t n) { bytes_in_ += n; }
  void IncrPacketsOut(uint64_t n) { packets_out_ += n; }
  void INcrBytesOut(uint64_t n) { bytes_out_ += n; }

  void execute(TimerWheel<SvcBase> *timer) override;

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

class alignas(64) RealSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<RealSvc>;

  ~RealSvc() override;

  // Return false if empty
  bool GetLocal(Tuple2 &tuple);
  void PutLocal(const Tuple2 &tuple);

 private:
  RealSvc(Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(tuple, metric), local_tuple_pool_() {
    bind_local_ips();
  }

  void bind_local_ips();

  std::stack<Tuple2, utils::vector<Tuple2>> local_tuple_pool_;

  friend class VirtSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RealSvc);
};

class alignas(64) VirtSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<VirtSvc>;
  using RsVec = utils::vector<RealSvc::Ptr>;

  ~VirtSvc() override = default;

  inline RealSvc::Ptr SelectRs(Tuple2 &ctuple);

 private:
  VirtSvc(Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(tuple, metric), rs_vec_(ALLOC) {
//    rs_vec_.reserve(CONFIG.svc.max_real_per_virtual);
  }

  RsVec rs_vec_;

  friend RealSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VirtSvc);
};

}  // namespace xlb
