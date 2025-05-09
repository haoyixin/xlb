#pragma once

#include "ports/common.h"

namespace xlb::ports {

class KNI final : public Port {
 public:
  KNI();
  ~KNI() override;

  struct Status Status() override {
    return {0, true, true, kni_ != nullptr};
  }

  inline uint16_t Recv(uint16_t qid, Packet **pkts, uint16_t cnt) override;
  inline uint16_t Send(uint16_t qid, Packet **pkts, uint16_t cnt) override;

 private:
  using M = utils::Metric<TS("xlb_ports"), TS("kni")>;

  rte_kni *kni_;
};

}  // namespace xlb::ports
