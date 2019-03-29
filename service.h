#pragma once

#include "utils/allocator.h"
#include "utils/unsafe_index_pool.h"
#include "utils/x_map.h"

#include "common.h"
#include "config.h"
#include "metric.h"
#include "tuple.h"

namespace xlb {

class SvcBase : public utils::intrusive_ref_counter<SvcBase> {
 public:
  SvcBase() = delete;
  virtual ~SvcBase() = default;

  SvcBase(Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : tuple_(tuple), metrics_(metric) {}

  auto &m_conns() const { return metrics_->conns().count(); }
  auto &m_packets() const { return metrics_->packets().count(); }
  auto &m_bytes() const { return metrics_->bytes().count(); }

  auto &tuple() const { return tuple_; }

 protected:
  Tuple2 tuple_;
  SvcMetrics::Ptr metrics_;
};

class RealSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<RealSvc>;

  RealSvc() = delete;
  ~RealSvc() override = default;

  inline void Destroy();

  inline static SvcMetrics::Ptr MakeMetrics(Tuple2 &tuple);

 private:
  RealSvc(class VirtSvc *vs, Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(tuple, metric), vs_(vs) {}

  class VirtSvc *vs_{};

  friend class VirtSvc;
};

class VirtSvc : public SvcBase {
 public:
  using Ptr = utils::intrusive_ptr<VirtSvc>;
  using RsVec = utils::vector<RealSvc::Ptr>;

  VirtSvc() = delete;
  ~VirtSvc() override = default;

  inline void Destroy();

  inline static SvcMetrics::Ptr MakeMetrics(Tuple2 &tuple);

  inline RealSvc::Ptr SelectRs(Tuple2 &ctuple);

  inline bool Full();
  inline RealSvc::Ptr FindRs(Tuple2 &tuple);

  // Confirm that the tuple does not exist and the rs_vec_ is not 'Full' is the
  // responsibility of the caller
  inline RealSvc::Ptr EmplaceRsUnsafe(Tuple2 &tuple, SvcMetrics::Ptr &metric);

 private:
  VirtSvc(class VTable *vtable, Tuple2 &tuple, SvcMetrics::Ptr &metric)
      : SvcBase(tuple, metric),
        rs_vec_(&utils::DefaultAllocator()),
        vtable_(vtable) {}

  RsVec rs_vec_;

  class VTable *vtable_;

  friend VTable;
  friend RealSvc;
};

}  // namespace xlb
