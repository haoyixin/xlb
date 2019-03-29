#include "table.h"
#include "metric.h"

namespace xlb {

VirtSvc::Ptr VTable::FindVs(Tuple2 &tuple) {
  VsMap::Entry *entry = vs_map_.Find(tuple);

  if (entry != nullptr)
    return entry->value;
  else
    return {};
}

VirtSvc::Ptr VTable::EmplaceVsUnsafe(Tuple2 &tuple, SvcMetrics::Ptr &metric) {
  DLOG_W(INFO) << "Adding VirtSvc ip: " << tuple;

  VsMap::Entry *entry =
      vs_map_.EmplaceUnsafe(tuple, new VirtSvc(this, tuple, metric));

  if (entry != nullptr)
    return entry->value;
  else
    return {};
}

bool VTable::Full() { return vs_map_.Size() >= CONFIG.svc.max_virtual_service; }

}  // namespace xlb
