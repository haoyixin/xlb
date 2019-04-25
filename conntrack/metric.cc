#include "conntrack/metric.h"

namespace xlb::conntrack {

namespace {

std::string combine_metric_name(std::string_view name,
                                std::string_view suffix) {
  return Format("%s_%s", std::string(name).c_str(),
                std::string(suffix).c_str());
}

std::string combine_prefix(std::string_view type) {
  return Format("xlb_service_%s", std::string(type).c_str());
}

std::string combine_svc_metric_name(const Tuple2 &tuple,
                                    std::string_view name) {
  return Format("%s_%d_%s", headers::ToIpv4Address(tuple.ip).c_str(),
                tuple.port.value(), std::string(name).c_str());
}

}  // namespace

bool Metric::Expose(std::string_view prefix, std::string_view name) {
  return (per_second_.expose_as(std::string(prefix),
                                combine_metric_name(name, "second")) == 0 &&
          count_.expose_as(std::string(prefix),
                           combine_metric_name(name, "count")) == 0);
}

void Metric::Hide() {
  per_second_.hide();
  count_.hide();
}

bool SvcMetrics::Expose(std::string_view type, const Tuple2 &tuple) {
  if (conns_.Expose(combine_prefix(type),
                    combine_svc_metric_name(tuple, "conns")) &&
      packets_in_.Expose(combine_prefix(type),
                         combine_svc_metric_name(tuple, "packets_in")) &&
      bytes_in_.Expose(combine_prefix(type),
                       combine_svc_metric_name(tuple, "bytes_in")) &&
      packets_out_.Expose(combine_prefix(type),
                          combine_svc_metric_name(tuple, "packets_out")) &&
      bytes_out_.Expose(combine_prefix(type),
                        combine_svc_metric_name(tuple, "bytes_out"))) {
    W_DVLOG(1) << "exposed metrics type: " << type << " service: " << tuple;
    return true;
  } else {
    Hide();
    W_LOG(ERROR) << "failed to expose metrics type: " << type
                 << " service: " << tuple;
    return false;
  }
}

void SvcMetrics::Hide() {
  if (!hidden_.test_and_set()) {
    conns_.Hide();
    packets_in_.Hide();
    bytes_in_.Hide();
    packets_out_.Hide();
    bytes_out_.Hide();
  }
}

/*
SvcMetrics::Ptr SvcMetricsPool::GetVs(const Tuple2 &tuple) {
  auto pair = vs_map_.try_emplace(tuple, nullptr);
  if (!pair.second) return pair.first->second;

  pair.first->second = new SvcMetrics("virt", tuple);
  return pair.first->second;
}

SvcMetrics::Ptr SvcMetricsPool::GetRs(const Tuple2 &tuple) {
  auto pair = rs_map_.try_emplace(tuple, nullptr);
  if (!pair.second) return pair.first->second;

  pair.first->second = new SvcMetrics("real", tuple);
  return pair.first->second;
}

void SvcMetricsPool::PurgeVs(const Tuple2 &tuple) {
  auto iter = vs_map_.find(tuple);

  // In fact, only the metric is hidden here and the ptr is deleted from the
  // pool. When the metric of the same tuple is created again, the expose is
  // guaranteed to be safe. The original metric will be automatically destroyed,
  // and rs is the same
  if (iter != vs_map_.end()) {
    iter->second->Hide();
    vs_map_.erase(iter);
  }
}

void SvcMetricsPool::PurgeRs(const Tuple2 &tuple) {
  auto iter = rs_map_.find(tuple);

  if (iter != rs_map_.end()) {
    iter->second->Hide();
    rs_map_.erase(iter);
  }
}
 */

}  // namespace xlb::conntrack
