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
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>

#include "daily/memory/AllocationCategory.hpp"
#include "daily/memory/VirtualMemory.hpp"

#if DAILY_MEMORY_CATEGORIES

namespace {

constexpr std::size_t kMiB = std::size_t{1} << 20;

std::int64_t committed(daily::AllocationCategory category) {
  return daily::allocation_category_usage(category).committed;
}

// Pushes categories 1 to levels, checking each becomes current.
void push_levels(std::size_t level, std::size_t levels) {
  if(level > levels) {
    return;
  }
  auto const category = static_cast<daily::AllocationCategory>(level);
  daily::AllocationCategoryScope scope(category);
  BOOST_TEST(daily::current_allocation_category() == category);
  push_levels(level + 1, levels);
  BOOST_TEST(daily::current_allocation_category() == category);
}

} // namespace

BOOST_AUTO_TEST_CASE(AllocationCategory_is_zero_with_nothing_pushed) {
  BOOST_TEST(daily::current_allocation_category() == 0);
}

BOOST_AUTO_TEST_CASE(AllocationCategory_scopes_nest) {
  {
    daily::AllocationCategoryScope const outer(10);
    BOOST_TEST(daily::current_allocation_category() == 10);
    {
      daily::AllocationCategoryScope inner(11);
      BOOST_TEST(daily::current_allocation_category() == 11);
    }
    BOOST_TEST(daily::current_allocation_category() == 10);
  }
  BOOST_TEST(daily::current_allocation_category() == 0);
}

BOOST_AUTO_TEST_CASE(AllocationCategory_holds_full_depth) {
  push_levels(1, daily::kAllocationCategoryDepth);
  BOOST_TEST(daily::current_allocation_category() == 0);
}

BOOST_AUTO_TEST_CASE(AllocationCategory_is_per_thread) {
  daily::AllocationCategoryScope scope(12);
  daily::AllocationCategory on_other_thread = 255;
  std::thread([&] { on_other_thread = daily::current_allocation_category(); }).join();
  BOOST_TEST(on_other_thread == 0);
  BOOST_TEST(daily::current_allocation_category() == 12);
}

// The reservation takes the category current when it's created, and its
// regions keep it after the scope ends.
BOOST_AUTO_TEST_CASE(AllocationCategory_charges_committed_pages_to_reservation) {
  constexpr daily::AllocationCategory kCategory = 201;
  std::size_t const page = daily::VirtualMemory::page_size();
  std::int64_t const before = committed(kCategory);

  daily::VirtualMemory const memory = [] {
    daily::AllocationCategoryScope scope(kCategory);
    return daily::VirtualMemory(64 * kMiB);
  }();
  auto const region = memory.region();

  region.commit(0, 4 * page);
  BOOST_TEST(committed(kCategory) - before == static_cast<std::int64_t>(4 * page));
  region.decommit(page, 2 * page);
  BOOST_TEST(committed(kCategory) - before == static_cast<std::int64_t>(2 * page));
  region.decommit(0, page);
  region.decommit(3 * page, page);
  BOOST_TEST(committed(kCategory) == before);
}

BOOST_AUTO_TEST_CASE(AllocationCategory_explicit_reservation_category_wins) {
  constexpr daily::AllocationCategory kScoped = 202;
  constexpr daily::AllocationCategory kExplicit = 203;
  std::size_t const page = daily::VirtualMemory::page_size();
  std::int64_t const scoped_before = committed(kScoped);
  std::int64_t const explicit_before = committed(kExplicit);

  daily::AllocationCategoryScope const scope(kScoped);
  daily::VirtualMemory const memory(64 * kMiB, kExplicit);
  memory.region().commit(0, page);
  BOOST_TEST(committed(kScoped) == scoped_before);
  BOOST_TEST(committed(kExplicit) - explicit_before == static_cast<std::int64_t>(page));
  memory.region().decommit(0, page);
  BOOST_TEST(committed(kExplicit) == explicit_before);
}

BOOST_AUTO_TEST_CASE(AllocationCategory_moves_with_reservation) {
  constexpr daily::AllocationCategory kCategory = 204;
  std::size_t const page = daily::VirtualMemory::page_size();
  std::int64_t const before = committed(kCategory);

  daily::VirtualMemory original(64 * kMiB, kCategory);
  daily::VirtualMemory moved(std::move(original));
  moved.region().commit(0, page);
  BOOST_TEST(committed(kCategory) - before == static_cast<std::int64_t>(page));
  moved.region().decommit(0, page);
  BOOST_TEST(committed(kCategory) == before);
}

#else

BOOST_AUTO_TEST_CASE(AllocationCategory_disabled_is_a_no_op) {
  daily::AllocationCategoryScope scope(10);
  BOOST_TEST(daily::current_allocation_category() == 0);
  BOOST_TEST(daily::allocation_category_usage(10).heap == 0);
  BOOST_TEST(daily::allocation_category_usage(10).committed == 0);
}

#endif
