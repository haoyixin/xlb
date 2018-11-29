// Copyright (c) 2018-2019, Qihoo 360 Technology Co. Ltd.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of their
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef XLB_UTILS_ALLOCATOR_H
#define XLB_UTILS_ALLOCATOR_H

#include <cstddef>
#include <rte_config.h>
#include <rte_malloc.h>
#include <new>

namespace xlb {
namespace utils {

template <typename T> class Allocator {
public:
  typedef T value_type;

  Allocator() = default;

  template <typename U> constexpr Allocator(const Allocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    if (n > std::size_t(-1) / sizeof(T))
      throw std::bad_alloc();
    if (auto p = static_cast<T *>(
            rte_malloc(NULL, n * sizeof(T), RTE_CACHE_LINE_SIZE)))
      return p;
    throw std::bad_alloc();
  }

  void deallocate(T *p, std::size_t n) noexcept { rte_free(p); }
};

} // namespace utils
} // namespace xlb

#endif // XLB_UTILS_ALLOCATOR_H
