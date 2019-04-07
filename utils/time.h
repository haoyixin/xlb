#pragma once

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include <sys/time.h>


namespace xlb::utils {

extern uint64_t tsc_hz;

extern uint64_t tsc_ns;
extern uint64_t tsc_us;
extern uint64_t tsc_ms;
extern uint64_t tsc_sec;

inline uint64_t Rdtsc() {
  union {
    uint64_t tsc_64;
    struct {
      uint32_t lo_32;
      uint32_t hi_32;
    };
  } tsc;

  asm volatile("rdtsc" : "=a"(tsc.lo_32), "=d"(tsc.hi_32));

  return tsc.tsc_64;
}

inline uint64_t TscToNs(uint64_t cycles) {
  return cycles / tsc_ns;
}

inline uint64_t TscToUs(uint64_t cycles) {
  return cycles / tsc_us;
}

inline uint64_t TscToMs(uint64_t cycles) {
  return cycles / tsc_ms;
}

inline uint64_t TscToSec(uint64_t cycles) {
  return cycles / tsc_sec;
}

/* Return current time in seconds since the Epoch.
 * This is consistent with Python's time.time() */
inline double GetEpochTime() {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

/* CPU time (in seconds) spent by the current thread.
 * Use it only relatively. */
inline double GetCpuTime() {
  struct timespec ts {};
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return ts.tv_sec + ts.tv_nsec / 1e9;
  } else {
    return GetEpochTime();
  }
}

}  // namespace xlb::utils
