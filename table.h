#pragma once

#include "utils/unsafe_index_pool.h"
#include "utils/x_map.h"

#include "config.h"
#include "metric.h"
#include "service.h"
#include "tuple.h"
#include "worker.h"

namespace xlb {

class VTable {
 public:
  using VsMap = utils::XMap<Tuple2, VirtSvc::Ptr>;

  VTable() : vs_map_(CONFIG.svc.max_virtual_service) {
    DLOG_W(INFO) << "Initialization VTable succeed";
  }
  ~VTable() = default;

  inline bool Full();
  inline VirtSvc::Ptr FindVs(Tuple2 &tuple);

  // Confirm that the tuple does not exist and the 'vs_map_' is not 'Full' is
  // the responsibility of the caller
  inline VirtSvc::Ptr EmplaceVsUnsafe(Tuple2 &tuple, SvcMetrics::Ptr &metric);

 private:
  VsMap vs_map_;

  friend VirtSvc;
};

class CTable {};

}  // namespace xlb
