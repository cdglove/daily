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
#include "daily/memory/AllocationCategory.hpp"

#if DAILY_MEMORY_CATEGORIES

#  include <array>
#  include <atomic>
#  include <boost/assert.hpp>

namespace daily {
namespace {

// Trivially constructible so that first use on a thread can't allocate,
// which would re-enter operator new. depth keeps counting past the end of
// stack so that pops still match pushes.
struct CategoryStack {
  std::uint8_t depth;
  std::array<AllocationCategory, kAllocationCategoryDepth> stack;
};
static_assert(sizeof(CategoryStack) == 64);

constinit thread_local CategoryStack t_categories = {};

// Constant initialised so allocations made before main are counted.
constinit std::array<std::atomic<std::int64_t>, kAllocationCategoryCount> g_heap_bytes = {};
constinit std::array<std::atomic<std::int64_t>, kAllocationCategoryCount>
    g_committed_bytes = {};

} // namespace

AllocationCategoryScope::AllocationCategoryScope(AllocationCategory category) {
  CategoryStack& s = t_categories;
  BOOST_ASSERT_MSG(
      s.depth < kAllocationCategoryDepth,
      "AllocationCategoryScope nested too deeply");
  if(s.depth < kAllocationCategoryDepth) {
    s.stack[s.depth] = category;
  }
  ++s.depth;
}

AllocationCategoryScope::~AllocationCategoryScope() {
  CategoryStack& s = t_categories;
  BOOST_ASSERT(s.depth > 0);
  --s.depth;
}

AllocationCategory current_allocation_category() {
  CategoryStack const& s = t_categories;
  if(s.depth == 0) {
    return 0;
  }
  std::size_t const top =
      s.depth < kAllocationCategoryDepth ? s.depth : kAllocationCategoryDepth;
  return s.stack[top - 1];
}

AllocationCategoryUsage allocation_category_usage(AllocationCategory category) {
  return {
      .heap = g_heap_bytes[category].load(std::memory_order_relaxed),
      .committed = g_committed_bytes[category].load(std::memory_order_relaxed)};
}

namespace detail {

void add_heap_bytes(AllocationCategory category, std::int64_t bytes) {
  g_heap_bytes[category].fetch_add(bytes, std::memory_order_relaxed);
}

void add_committed_bytes(AllocationCategory category, std::int64_t bytes) {
  g_committed_bytes[category].fetch_add(bytes, std::memory_order_relaxed);
}

} // namespace detail
} // namespace daily

#endif
