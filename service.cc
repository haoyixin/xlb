#include "service.h"
#include "config.h"
#include "table.h"

namespace xlb {

SvcMetrics::Ptr VirtSvc::MakeMetrics(xlb::Tuple2 &tuple) {
  DLOG_W(INFO) << "[VirtSvc::MakeMetrics]: " << tuple;
  return make_shared<expose_protected_ctor<SvcMetrics>>("virt", tuple);
}

RealSvc::Ptr VirtSvc::SelectRs(Tuple2 &ctuple) {
  if (unlikely(rs_vec_.empty())) return nullptr;

  // TODO: abstract selector

  return rs_vec_[std::hash<Tuple2>()(ctuple) % rs_vec_.size()];
}

RealSvc::Ptr VirtSvc::FindRs(Tuple2 &tuple) {
  auto iter = utils::find_if(
      rs_vec_, [&tuple](auto &rs) { return rs->tuple() == tuple; });

  if (iter != rs_vec_.end()) return *iter;

  return {};
}

RealSvc::Ptr VirtSvc::EmplaceRsUnsafe(Tuple2 &tuple, SvcMetrics::Ptr &metric) {
  DLOG_W(INFO) << "Adding RealSvc: " << tuple;
  return rs_vec_.emplace_back(new RealSvc(this, tuple, metric));
}

bool VirtSvc::Full() { return rs_vec_.size() >= CONFIG.svc.max_real_service; }

void VirtSvc::Destroy() {
  DLOG_W(INFO) << "Destroying VirtSvc: " << tuple_;
  vtable_->vs_map_.Remove(tuple_);
}

SvcMetrics::Ptr RealSvc::MakeMetrics(xlb::Tuple2 &tuple) {
  DLOG_W(INFO) << "[RealSvc::MakeMetrics]: " << tuple;
  return make_shared<expose_protected_ctor<SvcMetrics>>("real", tuple);
}

void RealSvc::Destroy() {
  if (vs_ != nullptr) {
    DLOG_W(INFO) << "Destroying RealSvc: " << tuple_
                 << " in VirtSvc: " << vs_->tuple_;
    utils::remove_erase_if(vs_->rs_vec_,
                           [this](auto &rs) { return rs->tuple() == tuple_; });
  }
}

}  // namespace xlb
