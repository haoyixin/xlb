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

}  // namespace xlb::conntrack
