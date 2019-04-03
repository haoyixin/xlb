#include <utility>

#include "config.h"
#include "service.h"
#include "table.h"
#include "worker.h"

#include "utils/allocator.h"
#include "utils/boost.h"

namespace xlb {

void SvcBase::CommitMetrics() {
  metrics_->conns_.count_ << conns_;
  metrics_->packets_in_.count_ << packets_in_;
  metrics_->bytes_in_.count_ << bytes_in_;
  metrics_->packets_out_.count_ << packets_out_;
  metrics_->bytes_out_.count_ << bytes_out_;
  ResetMetrics();
}

void SvcBase::ResetMetrics() {
  conns_ = 0;
  packets_in_ = 0;
  bytes_in_ = 0;
  packets_out_ = 0;
  bytes_out_ = 0;
}

RealSvc::Ptr VirtSvc::SelectRs(Tuple2 &ctuple) {
  if (unlikely(rs_vec_.empty())) return {};

  // TODO: abstract selector

  return rs_vec_[std::hash<Tuple2>()(ctuple) % rs_vec_.size()];
}

void RealSvc::BindLocalIp(be32_t ip) {
  // Meaningless when called in the master thread
  DCHECK(!MASTER);

  DLOG_W(INFO) << "Binding local ip: " << ToIpv4Address(ip)
               << " to RealSvc: " << tuple_;

  for (auto i :
       utils::irange((uint16_t)1024u, std::numeric_limits<uint16_t>::max())) {
    local_tuple_pool_.emplace(Tuple2{ip, be16_t{i}});
  }
}

bool RealSvc::GetLocal(Tuple2 &tuple) {
  if (local_tuple_pool_.empty())
    return false;

  tuple = local_tuple_pool_.top();

  local_tuple_pool_.pop();
  return true;
}

void RealSvc::PutLocal(const Tuple2 &tuple) { local_tuple_pool_.push(tuple); }

RealSvc::~RealSvc() {
  DLOG_W(INFO) << "Lazy destroying RealSvc: " << tuple_;
  // Means that it cannot be reused
  stable_->rs_map_.erase(tuple_);
}

}  // namespace xlb
