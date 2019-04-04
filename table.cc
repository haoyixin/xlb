#include "table.h"

namespace xlb {

VirtSvc::Ptr SvcTable::FindVs(Tuple2 &tuple) {
  VsMap::Entry *entry = vs_map_.Find(tuple);

  if (entry != nullptr)
    return entry->value;
  else
    return {};
}

VirtSvc::Ptr SvcTable::EnsureVsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric) {
  if (vs_map_.Size() >= CONFIG.svc.max_virtual_service) {
    last_error_ = "max_virtual_service exceeded";
    return {};
  }

  VsMap::Entry *entry = vs_map_.Emplace(tuple, nullptr);

  // There is a very small probability of returning nullptr
  if (entry != nullptr) {
    if (entry->value != nullptr) return entry->value;

    DLOG_W(INFO) << "Creating VirtSvc: " << tuple;
    entry->value = new VirtSvc(this, tuple, metric);
  } else {
    last_error_ = "virtual service map collision exceeded";
    return {};
  }
}

RealSvc::Ptr SvcTable::EnsureRsExist(Tuple2 &tuple, SvcMetrics::Ptr &metric) {
  if (vs_map_.Size() >= CONFIG.svc.max_real_service) {
    last_error_ = "max_real_service exceeded";
    return {};
  }

  auto pair = rs_map_.try_emplace(tuple, nullptr);

  if (!pair.second) {
    // TODO: is it safe ?
    // Here rs may already be detached, so we need to replace metric with
    // exposed one
    pair.first->second->metrics_ = metric;
    return pair.first->second;
  }

  DLOG_W(INFO) << "Creating RealSvc: " << tuple;
  pair.first->second = new RealSvc(this, tuple, metric);

  return pair.first->second;
}

bool SvcTable::EnsureRsAttachedTo(VirtSvc::Ptr &vs, RealSvc::Ptr &rs) {
  if (vs->rs_vec_.size() > CONFIG.svc.max_real_per_virtual) {
    last_error_ = "max_real_per_virtual exceeded";
    return false;
  }

  auto range = rs_vs_map_.equal_range(rs->tuple_);

  for (auto it = range.first; it != range.second; ++it)
    if (it->second == vs->tuple_) return true;

  DLOG_W(INFO) << "Attaching RealSvc: " << rs->tuple_
               << " to VirtSvc: " << vs->tuple_;
  // TODO: is it safe ?
  vs->rs_vec_.emplace_back(rs);
  rs_vs_map_.emplace(rs->tuple_, vs->tuple_);

  return true;
}

void SvcTable::EnsureRsDetachedFrom(Tuple2 &vs, Tuple2 &rs) {
  auto range = rs_vs_map_.equal_range(rs);

  auto attached = false;
  auto it = range.first;

  for (; it != range.second; ++it)
    if (it->second == vs) attached = true;

  if (attached) {
    auto vs_ptr = vs_map_.Find(vs)->value;
    utils::remove_erase_if(
        vs_ptr->rs_vec_, [&rs](auto &rs_ptr) { return rs_ptr->tuple_ == rs; });
    rs_vs_map_.erase(it);
  }
}

void SvcTable::EnsureVsNotExist(Tuple2 &vs) {
  VsMap::Entry *entry = vs_map_.Find(vs);
  if (entry == nullptr) return;

  DLOG_W(INFO) << "Erasing VirtSvc: " << vs;

  for (auto &rs : entry->value->rs_vec_) {
    auto range = rs_vs_map_.equal_range(rs->tuple_);
    auto it = range.first;
    for (; it != range.second; ++it)
      if (it->second == vs) break;

    rs_vs_map_.erase(it);
  }

  entry->value->rs_vec_.clear();
  vs_map_.Remove(vs);
}

bool SvcTable::RsDetached(Tuple2 &rs) { return rs_vs_map_.count(rs) == 0; }

Conn *ConnTable::Find(xlb::Tuple4 &tuple) {
  IdxMap ::Entry *entry = idx_map_.Find(tuple);

  if (entry == nullptr) return nullptr;

  return &conns_[entry->value];
}

inline Conn *ConnTable::EnsureConnExist(VirtSvc::Ptr &vs_ptr, Tuple2 &cli_tp) {
  Tuple4 orig_tp{cli_tp, vs_ptr->tuple()};

  IdxMap::Entry *orig_ent = idx_map_.Emplace(orig_tp, 0);

  if (unlikely(orig_ent == nullptr)) return nullptr;

  if (unlikely(orig_ent->value != 0)) return &conns_[orig_ent->value];

  if (unlikely(idx_pool_.empty())) {
    idx_map_.Remove(orig_tp);
    return nullptr;
  }

  // TODO: avoid adding reference counts
  auto rs_ptr = vs_ptr->SelectRs(cli_tp);
  if (unlikely(rs_ptr == nullptr)) {
    idx_map_.Remove(orig_tp);
    return nullptr;
  }

  Tuple2 loc_tp;
  if (unlikely(!rs_ptr->GetLocal(loc_tp))) {
    idx_map_.Remove(orig_tp);
    return nullptr;
  }

  Tuple4 rep_tp{rs_ptr->tuple(), loc_tp};

  auto idx = idx_pool_.top();
  idx_pool_.pop();

  // Since the original does not exist, we think that the reply is the same, so
  // use unsafe here
  IdxMap::Entry *rep_ent = idx_map_.EmplaceUnsafe(rep_tp, idx);

  if (unlikely(rep_ent == nullptr)) {
    idx_pool_.push(idx);
    rs_ptr->PutLocal(loc_tp);
    idx_map_.Remove(orig_tp);
    return nullptr;
  }

  Conn *conn = &conns_[idx];

  conn->local_ = loc_tp;
  conn->client_ = cli_tp;
  conn->virt_ = vs_ptr;
  conn->real_ = rs_ptr;

  timer_.Schedule(conn, 3);
  timer_.Reschedule(conn, 3);
  timer_.Advance(10);
//  timer_.ScheduleInRange(conn, 3, 5);
//  auto x = timer_.TicksToNextEvent();
  auto y = timer_.now();
}

}  // namespace xlb
