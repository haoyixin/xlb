#ifndef XLB_UTILS_TSC_H
#define XLB_UTILS_TSC_H

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include <sys/time.h>
#include <rte_cycles.h>

//TODO: using rte_cycles

extern uint64_t tsc_hz;

static inline uint64_t rdtsc(void) {
  return rte_get_timer_cycles();
}

static inline uint64_t tsc_to_ns(uint64_t cycles) {
  return cycles * 1e9 / rte_get_timer_hz();
}

static inline double tsc_to_us(uint64_t cycles) {
  return cycles * 1e6 / rte_get_timer_hz();
}

/* Return current time in seconds since the Epoch.
 * This is consistent with Python's time.time() */
static inline double get_epoch_time() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

/* CPU time (in seconds) spent by the current thread.
 * Use it only relatively. */
static inline double get_cpu_time() {
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return ts.tv_sec + ts.tv_nsec / 1e9;
  } else {
    return get_epoch_time();
  }
}

#endif // XLB_UTILS_TSC_H
