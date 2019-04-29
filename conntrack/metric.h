#pragma once

#include "conntrack/common.h"
#include "conntrack/tuple.h"

namespace xlb::conntrack {

class Metric {
 public:
  ~Metric() = default;

  void Hide();

 private:
  Metric() : count_(), per_second_(&count_) {}
  bool Expose(std::string_view prefix, std::string_view name);

  bvar::Adder<uint64_t> count_;
  bvar::PerSecond<bvar::Adder<uint64_t>> per_second_;

  friend class SvcMetrics;
  friend class SvcBase;
};

class SvcMetrics : public intrusive_ref_counter<SvcMetrics>, public INew {
 public:
  using Ptr = intrusive_ptr<SvcMetrics>;

  ~SvcMetrics() = default;

  // 'Expose' is not thread safe, but 'Hide' is different
  bool Expose(std::string_view type, const Tuple2 &tuple);
  void Hide();

  static Ptr Get() { return {new SvcMetrics()}; }

 protected:
  SvcMetrics() = default;

 private:
  std::atomic_flag hidden_ = ATOMIC_FLAG_INIT;

  Metric conns_;
  Metric packets_in_;
  Metric bytes_in_;
  Metric packets_out_;
  Metric bytes_out_;

  friend class SvcMetricsPool;
  friend class SvcBase;
};

}  // namespace xlb::conntrack
