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
  SvcBase(class SvcTable *stable, Tuple2 &tuple, SvcMetrics::Ptr &metric);
  ~SvcBase() override = default;

  auto &tuple() const { return tuple_; }

  void IncrConns(uint64_t n) { conns_ += n; }
  void IncrPacketsIn(uint64_t n) { packets_in_ += n; }
  void IncrBytesIn(uint64_t n) { bytes_in_ += n; }
  void IncrPacketsOut(uint64_t n) { packets_out_ += n; }
  void INcrBytesOut(uint64_t n) { bytes_out_ += n; }

  void execute(TimerWheel<SvcBase> *timer) override;

 protected:
  static constexpr uint64_t kCommitInterval = 3;

  inline void reset_metrics();
  inline void commit_metrics();

  Tuple2 tuple_;
  SvcMetrics::Ptr metrics_;
  class SvcTable *stable_;

 private:
  uint64_t conns_;
  uint64_t packets_in_;
  uint64_t bytes_in_;
  uint64_t packets_out_;
  uint64_t bytes_out_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SvcBase);
};

class RealSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<RealSvc>;

  ~RealSvc() override;

  void BindLocalIp(const be32_t ip);

  // Return false if empty
  bool GetLocal(Tuple2 &tuple);
  void PutLocal(const Tuple2 &tuple);

 private:
  RealSvc(class SvcTable *stable, Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(stable, tuple, metric),
        local_tuple_pool_(
            utils::make_vector<Tuple2>(std::numeric_limits<uint16_t>::max())) {}

  std::stack<Tuple2, utils::vector<Tuple2>> local_tuple_pool_;

  friend class VirtSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RealSvc);
};

class VirtSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<VirtSvc>;
  using RsVec = utils::vector<RealSvc::Ptr>;

  ~VirtSvc() override = default;

  inline RealSvc::Ptr SelectRs(Tuple2 &ctuple);

 private:
  VirtSvc(class SvcTable *stable, Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(stable, tuple, metric), rs_vec_(ALLOC) {
    rs_vec_.reserve(CONFIG.svc.max_real_per_virtual);
  }

  RsVec rs_vec_;

  friend RealSvc;
  friend class SvcTable;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VirtSvc);
};

}  // namespace xlb
