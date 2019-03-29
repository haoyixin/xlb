#pragma once

#include <string>
#include <string_view>

#include <bvar/bvar.h>

#include "headers/ip.h"

#include "common.h"
#include "tuple.h"

namespace xlb {

class Metric {
 public:
  Metric() = delete;
  ~Metric() = default;

  auto &count() const { return count_; }
  auto &per_second() const { return count_; }

 private:
  Metric(std::string_view prefix, std::string_view name)
      : count_(), per_second_(&count_) {
    count_.expose_as(std::string(prefix), combine_name(name, "count"));
    per_second_.expose_as(std::string(prefix), combine_name(name, "second"));

    DLOG_W(INFO) << "Exposed metrics prefix: " << prefix << " name: " << name;
  }

  static std::string combine_name(std::string_view name,
                                  std::string_view suffix) {
    return utils::Format("%s_%s", std::string(name).c_str(),
                         std::string(suffix).c_str());
  }

  bvar::Adder<uint64_t> count_;
  bvar::PerSecond<bvar::Adder<uint64_t>> per_second_;

  friend class SvcMetrics;
};

class SvcMetrics {
 public:
  using Ptr = shared_ptr<SvcMetrics>;

  SvcMetrics() = delete;
  ~SvcMetrics() = default;

  auto &conns() const { return conns_; }
  auto &packets() const { return packets_; }
  auto &bytes() const { return bytes_; }

 protected:
  SvcMetrics(std::string_view type, Tuple2 &tuple)
      : conns_(combine_prefix(type), combine_name(tuple, "conns")),
        packets_(combine_prefix(type), combine_name(tuple, "packets")),
        bytes_(combine_prefix(type), combine_name(tuple, "bytes")) {}

 private:
  static std::string combine_prefix(std::string_view type) {
    return utils::Format("xlb_service_%s", std::string(type).c_str());
  }

  static std::string combine_name(const Tuple2 &tuple, std::string_view name) {
    return utils::Format("%s_%d_%s", headers::ToIpv4Address(tuple.ip).c_str(),
                         tuple.port.value(), std::string(name).c_str());
  }

  Metric conns_;
  Metric packets_;
  Metric bytes_;

  friend class VirtSvc;
  friend class RealSvc;
};

}  // namespace xlb
