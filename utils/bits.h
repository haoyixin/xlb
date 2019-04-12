#pragma once

#include <glog/logging.h>
#include <x86intrin.h>

#include <algorithm>
#include <type_traits>

#include "common.h"

namespace xlb::utils {

// If the size exceeds 64, use std::bitset (The focus is [[gnu::packed]])
// TODO: ffs function
template <size_t N>
class [[gnu::packed]] bitset {
  static_assert(N > 0 && N <= 64);

 public:
  using data_type = std::conditional_t<
      N <= 8, uint8_t,
      std::conditional_t<N <= 16, uint16_t,
                         std::conditional_t<N <= 32, uint32_t, uint64_t>>>;

  template <typename T>
  bool operator[](T index) const {
    return (data_ & kOne << index) != kZero;
  }

  bool all() const {
    return (data_ & (kOne << N) - kOne) == (kOne << N) - kOne;
  }
  bool any() const { return (data_ & (kOne << N) - kOne) != kZero; }
  bool none() const { return (data_ & (kOne << N) - kOne) == kZero; }

  void set(size_t index) { data_ |= kOne << index; }
  void reset() { data_ = kZero; }
  void reset(size_t index) { data_ &= ~(kOne << index); }

  data_type raw() const { return data_; }

 private:
  static constexpr data_type kZero = std::numeric_limits<data_type>::min();
  static constexpr data_type kOne = kZero + 1;
  data_type data_ = kZero;
};

// TODO: add support for shifting at bit granularity
// Shifts `buf` to the left by `len` bytes and fills in with zeroes using
// std::memmove() and std::memset().
inline void ShiftBytesLeftSmall(uint8_t *buf, const size_t len, size_t shift) {
  shift = std::min(shift, len);
  memmove(buf, buf + shift, len - shift);
  memset(buf + len - shift, 0, shift);
}

// TODO: add support for shifting at bit granularity
// Shifts `buf` to the left by `len` bytes and fills in with zeroes.
// Will shift in 8-byte chunks if `shift` <= 8, othersize, uses
// std::memmove() and std::memset().
inline void ShiftBytesLeft(uint8_t *buf, const size_t len, const size_t shift) {
  if (len < sizeof(uint64_t) || shift > sizeof(uint64_t)) {
    return ShiftBytesLeftSmall(buf, len, shift);
  }

  uint8_t *tmp_buf = buf;
  size_t tmp_len = len;
  size_t inc = sizeof(uint64_t) - shift;
  while (tmp_len >= sizeof(uint64_t)) {
    auto *block = reinterpret_cast<uint64_t *>(tmp_buf);
    *block >>= shift * 8;
    tmp_buf += inc;
    tmp_len = buf + len - tmp_buf;
  }

  buf += len;
  if (static_cast<size_t>(buf - tmp_buf) > shift) {
    tmp_len = buf - tmp_buf - shift;
    memmove(tmp_buf, tmp_buf + shift, tmp_len);
    memset(buf - shift, 0, shift);
  }
}

// TODO: add support for shifting at bit granularity
// Shifts `buf` to the right by `len` bytes and fills in with zeroes using
// std::memmove() and std::memset().
inline void ShiftBytesRightSmall(uint8_t *buf, const size_t len, size_t shift) {
  shift = std::min(shift, len);
  memmove(buf + shift, buf, len - shift);
  memset(buf, 0, shift);
}

// TODO: add support for shifting at bit granularity
// Shifts `buf` to the right by `len` bytes and fills in with zeroes.
// Will shift in 8-byte chunks if `shift` <= 8, othersize, uses
// std::memmove() and std::memset().
inline void ShiftBytesRight(uint8_t *buf, const size_t len,
                            const size_t shift) {
  if (len < sizeof(uint64_t) || shift > sizeof(uint64_t)) {
    return ShiftBytesRightSmall(buf, len, shift);
  }

  uint8_t *tmp_buf = buf + len - sizeof(uint64_t);
  size_t dec = sizeof(uint64_t) - shift;
  size_t leftover = len;
  while (tmp_buf >= buf) {
    auto *block = reinterpret_cast<uint64_t *>(tmp_buf);
    *block <<= shift * 8;
    tmp_buf -= dec;
    leftover -= dec;
  }
  ShiftBytesRightSmall(buf, leftover, shift);
}

// Applies the `len`-byte bitmask `mask` to `buf`, in 1-byte chunks.
inline void MaskBytesSmall(uint8_t *buf, const uint8_t *mask,
                           const size_t len) {
  for (size_t i = 0; i < len; i++) {
    buf[i] &= mask[i];
  }
}

// Applies the `len`-byte bitmask `mask` to `buf`, in 8-byte chunks if able,
// otherwise, falls back to 1-byte chunks.
inline void MaskBytes64(uint8_t *buf, uint8_t const *mask, const size_t len) {
  size_t n = len / sizeof(uint64_t);
  size_t leftover = len - n * sizeof(uint64_t);
  auto *buf64 = reinterpret_cast<uint64_t *>(buf);
  const auto *mask64 = reinterpret_cast<const uint64_t *>(mask);
  for (size_t i = 0; i < n; i++) {
    buf64[i] &= mask64[i];
  }

  if (leftover) {
    buf = reinterpret_cast<uint8_t *>(buf64 + n);
    mask = reinterpret_cast<uint8_t const *>(mask64 + n);
    MaskBytesSmall(buf, mask, leftover);
  }
}

// Applies the `len`-byte bitmask `mask` to `buf`, in 16-byte chunks if able,
// otherwise, falls back to 8-byte chunks and possibly 1-byte chunks.
inline void MaskBytes(uint8_t *buf, uint8_t const *mask, const size_t len) {
  if (len <= sizeof(uint64_t)) {
    return MaskBytes64(buf, mask, len);
  }

  // TODO: AVX2 ?
  size_t n = len / sizeof(__m128i);
  size_t leftover = len - n * sizeof(__m128i);
  auto *buf128 = reinterpret_cast<__m128i *>(buf);
  const auto *mask128 = reinterpret_cast<const __m128i *>(mask);
  for (size_t i = 0; i < n; i++) {
    __m128i a = _mm_loadu_si128(buf128 + i);
    __m128i b = _mm_loadu_si128(mask128 + i);
    _mm_storeu_si128(buf128 + i, _mm_and_si128(a, b));
  }

  buf = reinterpret_cast<uint8_t *>(buf128 + n);
  mask = reinterpret_cast<uint8_t const *>(mask128 + n);
  if (leftover >= sizeof(uint64_t)) {
    MaskBytes64(buf, mask, leftover);
  } else {
    MaskBytesSmall(buf, mask, leftover);
  }
}

// Dangerous Helper! Use SetBitsHigh<T>() and SetBitsLow<T>() instead.
template <typename T, typename = std::enable_if<std::is_integral<T>::value>>
inline T _SetBitsHigh(size_t n) {
  return (T{1} << n) - 1;
}

// TODO: it would be nice if SetBitsHigh() supported blobs.
// Returns a mask for the first `n` bits of an integral type `T`, i.e. starting
// from the most significant bit toward the least significant. Returns a `T`
// with all bits set if `n` is greater than the number of bits in `T`.
// For example: SetBitsHigh<uint32_t>(9) will return 0xFF800000
template <typename T, typename = std::enable_if<std::is_integral<T>::value>>
inline T SetBitsHigh(size_t n) {
  if (unlikely(n == 0)) {
    return T{0};
  }
  return (n >= sizeof(T) * 8) ? ~T{0} : _SetBitsHigh<T>(n);
}

// TODO: it would be nice if SetBitsLow() supported blobs.
// Returns a mask for the last `n` bits of an integral type `T`, i.e. starting
// from the least significant bit toward the most signifant. Returns a `T` with
// all bits set if `n` is greater than the number of bits in `T`.
// For example: SetBitsLow<uint32_t>(9) will return 0x000001FF
template <typename T, typename = std::enable_if<std::is_integral<T>::value>>
inline T SetBitsLow(size_t n) {
  if (unlikely(n == 0)) {
    return T{0};
  }
  return (n >= sizeof(T) * 8) ? ~T{0} : ~_SetBitsHigh<T>((sizeof(T) * 8) - n);
}

}  // namespace xlb::utils
