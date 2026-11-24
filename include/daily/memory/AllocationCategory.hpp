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
#ifndef DAILY_MEMORY_ALLOCATIONCATEGORY_HPP
#define DAILY_MEMORY_ALLOCATIONCATEGORY_HPP
#pragma once

#include <cstddef>
#include <cstdint>

// Accounts memory against categories chosen by the application. A category
// is an ID with no meaning to this library; the application maps IDs to
// names. Code pushes a category with AllocationCategoryScope, and memory
// allocated while it's on top of the calling thread's stack is charged to
// it:
//
// - Heap memory is charged when the executable links daily::memory_new,
//   which replaces the global operator new and delete. The figures count the
//   bytes requested, not the allocator's overhead. Memory allocated with
//   malloc directly isn't counted.
// - Committed virtual memory is charged to the category of the VirtualMemory
//   reservation it's in. The figures count committed pages, which may not
//   all be resident.
//
// Building with DAILY_MEMORY_CATEGORIES=0 makes all of this a no-op, so code
// using it needn't change.
#ifndef DAILY_MEMORY_CATEGORIES
#  define DAILY_MEMORY_CATEGORIES 1
#endif

namespace daily {

// Category 0 is used when nothing has been pushed.
using AllocationCategory = std::uint8_t;

inline constexpr std::size_t kAllocationCategoryCount = 256;

// The number of categories each thread's stack holds. Nesting scopes
// deeper than this asserts; without assertions, the extra scopes leave the
// category unchanged.
inline constexpr std::size_t kAllocationCategoryDepth = 63;

// The memory charged to a category, in bytes. Memory is given back to the
// category it was charged to when it's freed, whichever category is current
// then.
struct AllocationCategoryUsage {
  std::int64_t heap = 0;
  std::int64_t committed = 0;
};

#if DAILY_MEMORY_CATEGORIES

// Makes category the calling thread's current category until destroyed.
class AllocationCategoryScope {
 public:
  explicit AllocationCategoryScope(AllocationCategory category);
  ~AllocationCategoryScope();

  AllocationCategoryScope(AllocationCategoryScope const&) = delete;
  AllocationCategoryScope& operator=(AllocationCategoryScope const&) = delete;
};

// The category on top of the calling thread's stack.
AllocationCategory current_allocation_category();

// The memory charged to category so far, by every thread.
AllocationCategoryUsage allocation_category_usage(AllocationCategory category);

namespace detail {
void add_heap_bytes(AllocationCategory category, std::int64_t bytes);
void add_committed_bytes(AllocationCategory category, std::int64_t bytes);
} // namespace detail

#else

// The same, doing nothing, for builds without memory categories.
class AllocationCategoryScope {
 public:
  explicit AllocationCategoryScope(AllocationCategory /* category */) {
  }

  AllocationCategoryScope(AllocationCategoryScope const&) = delete;
  AllocationCategoryScope& operator=(AllocationCategoryScope const&) = delete;
};

inline AllocationCategory current_allocation_category() {
  return 0;
}

inline AllocationCategoryUsage allocation_category_usage(
    AllocationCategory /* category */) {
  return {};
}

namespace detail {
inline void add_heap_bytes(AllocationCategory /* category */, std::int64_t /* bytes */) {
}
inline void add_committed_bytes(
    AllocationCategory /* category */,
    std::int64_t /* bytes */) {
}
} // namespace detail

#endif

} // namespace daily

#endif // DAILY_MEMORY_ALLOCATIONCATEGORY_HPP
