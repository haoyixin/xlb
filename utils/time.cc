#include "utils/time.h"

namespace xlb::utils {

uint64_t tsc_hz;

uint64_t tsc_ns;
uint64_t tsc_us;
uint64_t tsc_ms;
uint64_t tsc_sec;

class TscHzSetter {
 public:
  TscHzSetter() {
    tsc_hz = rte_get_tsc_hz();

    tsc_sec = tsc_hz;
    tsc_ms = tsc_sec / 1000;
    tsc_us = tsc_ms / 1000;
    tsc_ns = tsc_us / 1000;

    F_DLOG(INFO) << "tsc_hz: " << tsc_hz;
  }
};

}  // namespace xlb::utils
