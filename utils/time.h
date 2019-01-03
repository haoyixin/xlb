#ifndef XLB_UTILS_TSC_H
#define XLB_UTILS_TSC_H

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include <rte_cycles.h>
#include <sys/time.h>

namespace xlb {
namespace utils {

static inline uint64_t Rdtsc(void) { return rte_get_timer_cycles(); }

static inline uint64_t TscToNs(uint64_t cycles) {
  return cycles * 1e9 / rte_get_timer_hz();
}

static inline double TscToUs(uint64_t cycles) {
  return cycles * 1e6 / rte_get_timer_hz();
}

/* Return current time in seconds since the Epoch.
 * This is consistent with Python's time.time() */
static inline double GetEpochTime() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

/* CPU time (in seconds) spent by the current thread.
 * Use it only relatively. */
static inline double GetCpuTime() {
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return ts.tv_sec + ts.tv_nsec / 1e9;
  } else {
    return GetEpochTime();
  }
}

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_TSC_H
