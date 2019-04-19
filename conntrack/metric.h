#pragma once

#include "conntrack/common.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

class Metric {
 public:
  ~Metric() = default;

  void Hide();

 private:
  Metric(std::string_view prefix, std::string_view name);

  bvar::Adder<uint64_t> count_;
  bvar::PerSecond<bvar::Adder<uint64_t>> per_second_;

  friend class SvcMetrics;
  friend class SvcBase;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Metric);
};

class SvcMetrics : public intrusive_ref_counter<SvcMetrics>, public INew {
 public:
  using Ptr = intrusive_ptr<SvcMetrics>;

  ~SvcMetrics() = default;

  void Hide();

 protected:
  SvcMetrics(std::string_view type, Tuple2 &tuple);

 private:
  Metric conns_;
  Metric packets_in_;
  Metric bytes_in_;
  Metric packets_out_;
  Metric bytes_out_;

  friend class SvcMetricsPool;
  friend class SvcBase;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SvcMetrics);
};

class SvcMetricsPool {
 public:
  SvcMetricsPool() : vs_map_(ALLOC), rs_map_(ALLOC) {
    vs_map_.reserve(CONFIG.svc.max_virtual_service);
    rs_map_.reserve(CONFIG.svc.max_real_service);
  }
  ~SvcMetricsPool() = default;

  SvcMetrics::Ptr GetVs(Tuple2 &tuple);
  SvcMetrics::Ptr GetRs(Tuple2 &tuple);

  void PurgeVs(Tuple2 &tuple);
  void PurgeRs(Tuple2 &tuple);

 private:
  using SvcMetricsMap = unordered_map<Tuple2, SvcMetrics::Ptr>;

  SvcMetricsMap vs_map_;
  SvcMetricsMap rs_map_;

  DISALLOW_COPY_AND_ASSIGN(SvcMetricsPool);
};

}  // namespace xlb::conntrack
