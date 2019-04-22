#include "conntrack/table.h"

namespace xlb::conntrack {

VirtSvc::Ptr SvcTable::FindVs(const Tuple2 &tuple) {
  VsMap::Entry *entry = vs_map_.Find(tuple);

  if (entry != nullptr)
    return entry->value;
  else
    return {};
}

VirtSvc::Ptr SvcTable::EnsureVsExist(const Tuple2 &tuple,
                                     SvcMetrics::Ptr metric) {
  if (vs_map_.Size() >= CONFIG.svc.max_virtual_service) {
    last_error_ = "max_virtual_service exceeded";
    return {};
  }

  VsMap::Entry *entry = vs_map_.Emplace(tuple, nullptr);

  // There is a very small probability of returning nullptr
  if (entry != nullptr) {
    if (entry->value == nullptr) {
      W_DVLOG(1) << "creating VirtSvc: " << tuple;
      entry->value = new VirtSvc(tuple, metric);
    }
    // TODO: safe ?
    //    entry->value->metrics_ = metric;
    return entry->value;
  } else {
    last_error_ = "virtual service map collision exceeded";
    return {};
  }
}

RealSvc::Ptr SvcTable::EnsureRsExist(const Tuple2 &tuple,
                                     SvcMetrics::Ptr metric) {
  if (rs_vs_map_.size() >= CONFIG.svc.max_real_service) {
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

  W_DVLOG(1) << "creating RealSvc: " << tuple;
  pair.first->second = new RealSvc(tuple, metric);

  return pair.first->second;
}

bool SvcTable::EnsureRsAttachedTo(VirtSvc::Ptr vs, RealSvc::Ptr rs) {
  if (vs->rs_vec_.size() > CONFIG.svc.max_real_per_virtual) {
    last_error_ = "max_real_per_virtual exceeded";
    return false;
  }

  auto range = rs_vs_map_.equal_range(rs->tuple_);

  for (auto it = range.first; it != range.second; ++it)
    if (it->second == vs->tuple_) return true;

  W_DVLOG(1) << "attaching RealSvc: " << rs->tuple_
             << " to VirtSvc: " << vs->tuple_;
  // TODO: is it safe ?
  vs->rs_vec_.emplace_back(rs);
  rs_vs_map_.emplace(rs->tuple_, vs->tuple_);

  return true;
}

void SvcTable::EnsureRsDetachedFrom(const Tuple2 &vs, const Tuple2 &rs) {
  auto range = rs_vs_map_.equal_range(rs);

  auto attached = false;
  auto it = range.first;

  for (; it != range.second; ++it)
    if (it->second == vs) attached = true;

  if (attached) {
    auto vs_ptr = vs_map_.Find(vs)->value;
    remove_erase_if(vs_ptr->rs_vec_,
                    [&rs](auto &rs_ptr) { return rs_ptr->tuple_ == rs; });
    rs_vs_map_.erase(it);
  }
}

void SvcTable::EnsureVsNotExist(const Tuple2 &vs) {
  VsMap::Entry *entry = vs_map_.Find(vs);
  if (entry == nullptr) return;

  W_DVLOG(1) << "erasing VirtSvc: " << vs;

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

bool SvcTable::RsDetached(const Tuple2 &rs) {
  return rs_vs_map_.count(rs) == 0;
}

Conn *ConnTable::Find(Tuple4 &tuple) {
  IdxMap ::Entry *entry = idx_map_.Find(tuple);

  if (entry == nullptr) return nullptr;

  return &conns_[entry->value];
}

Conn *ConnTable::EnsureConnExist(VirtSvc::Ptr vs_ptr, const Tuple2 &cli_tp) {
  IdxMap::Entry *orig_ent = idx_map_.Emplace({cli_tp, vs_ptr->tuple()}, 0);

  // Collision exceeded
  if (unlikely(orig_ent == nullptr)) return nullptr;

  // Already exist, return directly
  if (unlikely(orig_ent->value != 0)) return &conns_[orig_ent->value];

  // The table is full
  if (unlikely(idx_pool_.empty())) {
    // Clean up the map
    idx_map_.Remove(orig_ent->key);
    return nullptr;
  }

  // TODO: avoid adding reference counts
  auto rs_ptr = vs_ptr->SelectRs(cli_tp);
  // No rs attached to vs
  if (unlikely(!rs_ptr)) {
    // Clean up the map
    idx_map_.Remove(orig_ent->key);
    return nullptr;
  }

  Tuple2 loc_tp;
  // Local tuple runs out
  if (unlikely(!rs_ptr->GetLocal(loc_tp))) {
    // Clean up the map
    idx_map_.Remove(orig_ent->key);
    return nullptr;
  }

  auto idx = idx_pool_.top();
  idx_pool_.pop();

  // Since the original does not exist, we think that the reply is the same, so
  // use unsafe here for performance
  IdxMap::Entry *rep_ent =
      idx_map_.EmplaceUnsafe({rs_ptr->tuple(), loc_tp}, idx);

  // Collision exceeded
  if (unlikely(rep_ent == nullptr)) {
    // Release index
    idx_pool_.push(idx);
    // Release local tuple
    rs_ptr->PutLocal(loc_tp);
    // Clean up the map
    idx_map_.Remove(orig_ent->key);
    return nullptr;
  }

  Conn *conn = &conns_[idx];

  conn->local_ = loc_tp;
  conn->client_ = cli_tp;
  conn->virt_ = vs_ptr;
  conn->real_ = rs_ptr;

  conn->state_ = TCP_CONNTRACK_SYN_SENT;

  W_DVLOG(2) << "creating Conn: " << *conn;
}

}  // namespace xlb::conntrack
