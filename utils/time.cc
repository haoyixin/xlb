#include "utils/time.h"

#include <rte_cycles.h>


namespace xlb::utils {

uint64_t tsc_hz;

class TscHzSetter {
 public:
  TscHzSetter() {
    tsc_hz = rte_get_tsc_hz();
  }
} _dummy;

}
