#pragma once

#include <string>

#include <x86intrin.h>

#include <glog/logging.h>

#include "utils/common.h"
#include "utils/format.h"

#if !__SSE4_2__
#error CPU must be at least Intel Nehalem equivalent (SSE4.2 required)
#endif

namespace xlb {

#define __xmm_aligned __attribute__((aligned(16)))
#define __ymm_aligned __attribute__((aligned(32)))
#define __zmm_aligned __attribute__((aligned(64)))

}  // namespace xlb

namespace xlb::utils {

inline std::string m128i_to_str(__m128i a) {
  union {
    __m128i vec;
    uint32_t b[4];
  };

  vec = a;
  return Format("[%08x %08x %08x %08x]", b[0], b[1], b[2], b[3]);
}

inline __m128i gather_m128i(void *a, void *b) {
#if 1
  // faster (in a tight loop test. sometimes slower...)
  __m128i t = _mm_loadl_epi64((__m128i *)a);
  return (__m128i)_mm_loadh_pd((__m128d)t, (double *)b);
#else
  return _mm_set_epi64x(*((uint64_t *)b), *((uint64_t *)a));
#endif
}

#if __AVX__

inline std::string m256i_to_str(__m256i a) {
  union {
    __m256i vec;
    uint32_t b[8];
  };

  vec = a;
  return Format("[%08x %08x %08x %08x %08x %08x %08x %08x]", b[0], b[1], b[2],
                b[3], b[4], b[5], b[6], b[7]);
}

inline __m256d concat_two_m128d(__m128d lo, __m128d hi) {
#if 1
  // faster
  return _mm256_insertf128_pd(_mm256_castpd128_pd256(lo), hi, 1);
#else
  return _mm256_permute2f128_si256(_mm256_castsi128_si256(lo),
                                   _mm256_castsi128_si256(hi), (2 << 4) | 0);
#endif
}

inline __m256i concat_two_m128i(__m128i lo, __m128i hi) {
#if __AVX2__
  return _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1);
#else
  return _mm256_insertf128_si256(_mm256_castsi128_si256(lo), hi, 1);
#endif
}

/*
inline uint64_t m128i_extract_u64(__m128i a, int i) {
#if __x86_64
DCHECK(i == 0 || i == 1) << "selector must be either 0 or 1";

// While this looks silly, otherwise g++ will complain on -O0
if (i == 0) {
return _mm_extract_epi64(a, 0);
} else {
return _mm_extract_epi64(a, 1);
}
#else
// In 32-bit machines, _mm_extract_epi64() is not supported
union {
__m128i vec;
uint64_t b[2];
};

vec = a;
return b[i];
#endif
}
 */

#endif  // __AVX__

}  // namespace xlb::utils
