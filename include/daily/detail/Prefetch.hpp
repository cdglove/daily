// Copyright (c) 2026 Christopher Glover.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef DAILY_DETAIL_PREFETCH_HPP
#define DAILY_DETAIL_PREFETCH_HPP
#pragma once

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#  include <xmmintrin.h>
#endif

namespace daily::detail {

// Hints that memory at p will be read soon.
inline void prefetch(void const* p) {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(p);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  _mm_prefetch(static_cast<char const*>(p), _MM_HINT_T0);
#else
  (void)p;
#endif
}

} // namespace daily::detail

#endif // DAILY_DETAIL_PREFETCH_HPP
