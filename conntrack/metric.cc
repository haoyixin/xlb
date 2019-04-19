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

Metric::Metric(std::string_view prefix, std::string_view name)
    : count_(), per_second_(&count_) {
  count_.expose_as(std::string(prefix), combine_metric_name(name, "count"));
  per_second_.expose_as(std::string(prefix),
                        combine_metric_name(name, "second"));

  W_DLOG(INFO) << "Exposed metrics prefix: " << prefix << " name: " << name;
}

void Metric::Hide() {
  per_second_.hide();
  count_.hide();
}

SvcMetrics::SvcMetrics(std::string_view type, Tuple2 &tuple)
    : conns_(combine_prefix(type), combine_svc_metric_name(tuple, "conns")),
      packets_in_(combine_prefix(type),
                  combine_svc_metric_name(tuple, "packets_in")),
      bytes_in_(combine_prefix(type),
                combine_svc_metric_name(tuple, "bytes_in")),
      packets_out_(combine_prefix(type),
                   combine_svc_metric_name(tuple, "packets_out")),
      bytes_out_(combine_prefix(type),
                 combine_svc_metric_name(tuple, "bytes_out")) {}

void SvcMetrics::Hide() {
  conns_.Hide();
  packets_in_.Hide();
  bytes_in_.Hide();
  packets_out_.Hide();
  bytes_out_.Hide();
}

SvcMetrics::Ptr SvcMetricsPool::GetVs(Tuple2 &tuple) {
  auto pair = vs_map_.try_emplace(tuple, nullptr);
  if (!pair.second) return pair.first->second;

  pair.first->second = new SvcMetrics("virt", tuple);
  return pair.first->second;
}

SvcMetrics::Ptr SvcMetricsPool::GetRs(Tuple2 &tuple) {
  auto pair = rs_map_.try_emplace(tuple, nullptr);
  if (!pair.second) return pair.first->second;

  pair.first->second = new SvcMetrics("real", tuple);
  return pair.first->second;
}

void SvcMetricsPool::PurgeVs(Tuple2 &tuple) {
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

void SvcMetricsPool::PurgeRs(Tuple2 &tuple) {
  auto iter = rs_map_.find(tuple);

  if (iter != rs_map_.end()) {
    iter->second->Hide();
    rs_map_.erase(iter);
  }
}

}  // namespace xlb::conntrack
