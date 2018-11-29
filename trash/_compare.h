//
// Created by haoyixin on 11/22/18.
//

#ifndef XLB_UTILS_COMPARE_H
#define XLB_UTILS_COMPARE_H

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <x86intrin.h>

namespace xlb {
namespace utils {

//#if __AVX__
//#define MAX_INTR 32
//#else
//#define MAX_INTR 16
//#endif
//
//constexpr size_t head_size(const size_t n) {
//  for (size_t i = MAX_INTR; i > 0; i /= 2) {
//    if (n > i)
//      return i;
//  }
//}
//
//constexpr size_t tail_size(const size_t n) { return n - head_size(n); }
//
//template <size_t n> struct obj { uint8_t data[n]; };
//
//template <size_t n> inline int memcmp_(const obj<n> *lhs, const obj<n> *rhs) {
//  constexpr auto hs = head_size(n);
//  constexpr auto ts = tail_size(n);
//  return memcmp_((const obj<hs> *)lhs, (const obj<hs> *)rhs) ||
//         memcmp_((const obj<ts> *)(lhs->data + hs),
//                 (const obj<ts> *)(rhs->data + hs));
//}
//
//
//#if __AVX__
//template <> inline int memcmp_(const obj<32> *lhs, const obj<32> *rhs) {
//  const __m256i l = _mm256_loadu_si256((const __m256i *)lhs);
//  const __m256i r = _mm256_loadu_si256((const __m256i *)rhs);
//  return !_mm256_testc_si256(l, r);
//}
//#endif
//
//template <> inline int memcmp_(const obj<16> *lhs, const obj<16> *rhs) {
//  const __m128i l = _mm_loadu_si128((const __m128i *)lhs);
//  const __m128i r = _mm_loadu_si128((const __m128i *)rhs);
//  const __m128i x = _mm_xor_si128(l, r);
//  return !_mm_test_all_zeros(x, x);
//}
//
//template <> inline int memcmp_(const obj<8> *lhs, const obj<8> *rhs) {
//  return *(const uint64_t *)lhs != *(const uint64_t *)rhs;
//}
//
//template <> inline int memcmp_(const obj<4> *lhs, const obj<4> *rhs) {
//  return *(const uint32_t *)lhs != *(const uint32_t *)rhs;
//}
//
//template <> inline int memcmp_(const obj<2> *lhs, const obj<2> *rhs) {
//  return *(const uint16_t *)lhs != *(const uint16_t *)rhs;
//}
//
//template <> inline int memcmp_(const obj<1> *lhs, const obj<1> *rhs) {
//  return lhs->data[0] != rhs->data[0];
//}
//
//template <typename T> inline int MemoryCompare(const T &lhs, const T &rhs) {
//  return memcmp_((const obj<sizeof(T)> *)&lhs, (const obj<sizeof(T)> *)&rhs);
//}

#if !__SSE4_2__
#error CPU must be at least Intel Nehalem equivalent (SSE4.2 required)
#endif

#if __AVX__
#define MAX_INTR 32
#else
#define MAX_INTR 16
#endif

constexpr size_t head_size(const size_t n) {
  for (size_t i = MAX_INTR; i > 0; i /= 2) {
    if (n > i)
      return i;
  }
}

constexpr size_t tail_size(const size_t n) { return n - head_size(n); }

template <size_t n> struct obj { uint8_t data[n]; };

template <size_t n> inline int memcmp_(const obj<n> *lhs, const obj<n> *rhs) {
  constexpr auto hs = head_size(n);
  constexpr auto ts = tail_size(n);
  return memcmp_((const obj<hs> *)lhs, (const obj<hs> *)rhs) ||
         memcmp_((const obj<ts> *)(lhs->data + hs),
                 (const obj<ts> *)(rhs->data + hs));
}

#if __AVX__
template <> inline int memcmp_(const obj<32> *lhs, const obj<32> *rhs) {
  const __m256i l = _mm256_loadu_si256((const __m256i *)lhs);
  const __m256i r = _mm256_loadu_si256((const __m256i *)rhs);
  return !_mm256_testc_si256(l, r);
}
#endif

template <> inline int memcmp_(const obj<16> *lhs, const obj<16> *rhs) {
  const __m128i l = _mm_loadu_si128((const __m128i *)lhs);
  const __m128i r = _mm_loadu_si128((const __m128i *)rhs);
  const __m128i x = _mm_xor_si128(l, r);
  return !_mm_test_all_zeros(x, x);
}

template <> inline int memcmp_(const obj<8> *lhs, const obj<8> *rhs) {
  return *(const uint64_t *)lhs != *(const uint64_t *)rhs;
}

template <> inline int memcmp_(const obj<4> *lhs, const obj<4> *rhs) {
  return *(const uint32_t *)lhs != *(const uint32_t *)rhs;
}

template <> inline int memcmp_(const obj<2> *lhs, const obj<2> *rhs) {
  return *(const uint16_t *)lhs != *(const uint16_t *)rhs;
}

template <> inline int memcmp_(const obj<1> *lhs, const obj<1> *rhs) {
  return lhs->data[0] != rhs->data[0];
}

template <typename T> inline int MemoryCompare(const T &lhs, const T &rhs) {
  return memcmp_((const obj<sizeof(T)> *)&lhs, (const obj<sizeof(T)> *)&rhs);
}

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_COMPARE_H
