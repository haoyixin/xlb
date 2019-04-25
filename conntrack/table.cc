#include "conntrack/table.h"

namespace xlb::conntrack {

VirtSvc::Ptr SvcTable::FindVs(const Tuple2 &tuple) {
  VsMap::Entry *entry = vs_map_.Find(tuple);

  if (entry != nullptr)
    return entry->value;
  else
    return {};
}

RealSvc::Ptr SvcTable::FindRs(const Tuple2 &tuple) {
  auto iter = rs_map_.find(tuple);

  if (iter != rs_map_.end()) return {iter->second};

  return {};
}

VirtSvc::Ptr SvcTable::AddVs(const Tuple2 &tuple) {
  VsMap::Entry *entry = vs_map_.EmplaceUnsafe(tuple, nullptr);

  // There is a very small probability of returning nullptr
  if (!entry) {
    last_error_ = "number of conflicts exceeds the limit";
    return {};
  }

  DCHECK(!entry->value);

  W_DVLOG(1) << "creating VirtSvc: " << tuple;
  entry->value = new VirtSvc(tuple);

  return {entry->value};
}

RealSvc::Ptr SvcTable::AddRs(const Tuple2 &tuple) {
  auto pair = rs_map_.try_emplace(tuple, nullptr);

  // Recycling rs that have not been released yet
  if (pair.second) {
    W_DVLOG(1) << "creating RealSvc: " << tuple;
    pair.first->second = new RealSvc(tuple);
  }

  DCHECK_NOTNULL(pair.first->second);

  return {pair.first->second};
}

std::pair<bool, SvcTable::Hint> SvcTable::RsAttached(VirtSvc::Ptr vs,
                                                     RealSvc::Ptr rs) {
  DCHECK_NOTNULL(vs);
  DCHECK_NOTNULL(rs);

  auto range = rs_vs_map_.equal_range(rs->tuple_);

  for (auto it = range.first; it != range.second; ++it)
    if (it->second == vs->tuple_) return {true, it};

  return {false, {}};
}

/*
bool SvcTable::RsDetached(RealSvc::Ptr rs) {
  return rs_vs_map_.count(rs->tuple_) == 0;
}
 */

RealSvc::Ptr SvcTable::AttachRs(VirtSvc::Ptr vs, RealSvc::Ptr rs) {
  DCHECK_NOTNULL(vs);
  DCHECK_NOTNULL(rs);

  W_DVLOG(1) << "attaching RealSvc: " << rs->tuple_
             << " to VirtSvc: " << vs->tuple_;
  // TODO: is it safe ?
  vs->rs_vec_.emplace_back(rs);
  rs_vs_map_.emplace(rs->tuple_, vs->tuple_);

  return rs;
}

void SvcTable::DetachRs(VirtSvc::Ptr vs, RealSvc::Ptr rs, Hint it) {
  DCHECK_NOTNULL(vs);
  DCHECK_NOTNULL(rs);
  DCHECK(it != rs_vs_map_.end());

  W_DVLOG(1) << "detaching RealSvc: " << rs->tuple_
             << " to VirtSvc: " << vs->tuple_;

  remove_erase_if(vs->rs_vec_,
                  [&rs](auto &_rs) { return _rs->tuple_ == rs->tuple_; });
  rs_vs_map_.erase(it);
}

void SvcTable::RemoveVs(VirtSvc::Ptr vs) {
  DCHECK_NOTNULL(vs);

  W_DVLOG(1) << "removing VirtSvc: " << vs->tuple_;

  for (auto &rs : vs->rs_vec_) {
    auto range = rs_vs_map_.equal_range(rs->tuple_);
    auto it = range.first;
    for (; it != range.second; ++it)
      if (it->second == vs->tuple_) break;

    rs_vs_map_.erase(it);
  }

  vs->rs_vec_.clear();
  vs_map_.Remove(vs->tuple_);
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
