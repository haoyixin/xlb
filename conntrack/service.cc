#include "conntrack/service.h"
#include "conntrack/table.h"

namespace xlb::conntrack {

SvcBase::SvcBase(const Tuple2 &tuple) : tuple_(tuple), metrics_(nullptr) {
  reset_metrics();
  STABLE.timer_.ScheduleInRange(this, kTimerStart * tsc_ms, kTimerEnd * tsc_ms);
}

void SvcBase::commit_metrics() {
  metrics_->conns_.count_ << conns_;
  metrics_->packets_in_.count_ << packets_in_;
  metrics_->bytes_in_.count_ << bytes_in_;
  metrics_->packets_out_.count_ << packets_out_;
  metrics_->bytes_out_.count_ << bytes_out_;
  reset_metrics();
}

void SvcBase::reset_metrics() {
  conns_ = 0;
  packets_in_ = 0;
  bytes_in_ = 0;
  packets_out_ = 0;
  bytes_out_ = 0;
}

void SvcBase::execute(TimerWheel<SvcBase> *timer) {
  W_DVLOG(4) << "commit metrics of: " << tuple_;

  commit_metrics();
  STABLE.timer_.ScheduleInRange(this, kTimerStart * tsc_ms, kTimerEnd * tsc_ms);
}

RealSvc::Ptr VirtSvc::SelectRs(const Tuple2 &ctuple) {
  if (unlikely(rs_vec_.empty())) return {};

  // TODO: abstract selector

  return rs_vec_[std::hash<Tuple2>()(ctuple) % rs_vec_.size()];
}

void RealSvc::bind_local_ips() {
  // Meaningless when called in the master thread
  if (!W_SLAVE) return;

  local_tuple_pool_ = STABLE.Prototype();
}

bool RealSvc::GetLocal(Tuple2 &tuple) {
  if (local_tuple_pool_.empty()) return false;

  tuple = local_tuple_pool_.top();

  local_tuple_pool_.pop();
  return true;
}

void RealSvc::PutLocal(const Tuple2 &tuple) { local_tuple_pool_.push(tuple); }

RealSvc::~RealSvc() {
  W_DVLOG(1) << "lazy destroying: " << tuple_;
  // Means that it cannot be reused
  STABLE.rs_map_.erase(tuple_);
}

}  // namespace xlb::conntrack
