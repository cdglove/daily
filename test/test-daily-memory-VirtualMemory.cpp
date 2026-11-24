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

#include <algorithm>
#include <cstddef>
#include <utility>

#include "daily/memory/VirtualMemory.hpp"

namespace {
constexpr std::size_t kGiB = std::size_t{1} << 30;
}

BOOST_AUTO_TEST_CASE(VirtualMemory_page_size_is_a_power_of_two) {
  std::size_t const page = daily::VirtualMemory::page_size();
  BOOST_TEST(page >= 4096u);
  BOOST_TEST((page & (page - 1)) == 0u);
  BOOST_TEST(daily::VirtualMemory::round_to_page(0) == 0u);
  BOOST_TEST(daily::VirtualMemory::round_to_page(1) == page);
  BOOST_TEST(daily::VirtualMemory::round_to_page(page) == page);
  BOOST_TEST(daily::VirtualMemory::round_to_page(page + 1) == 2 * page);
}

BOOST_AUTO_TEST_CASE(VirtualMemory_default_is_empty) {
  daily::VirtualMemory memory;
  BOOST_TEST(memory.data() == nullptr);
  BOOST_TEST(memory.size() == 0u);
}

// Reserving takes address space only, so a reservation far larger than the
// machine's memory succeeds.
BOOST_AUTO_TEST_CASE(VirtualMemory_reserves_more_than_memory) {
  daily::VirtualMemory memory(256 * kGiB);
  BOOST_TEST(memory.data() != nullptr);
  BOOST_TEST(memory.size() >= 256 * kGiB);
}

BOOST_AUTO_TEST_CASE(VirtualMemory_rounds_size_up) {
  daily::VirtualMemory memory(1);
  BOOST_TEST(memory.size() >= daily::VirtualMemory::page_size());
  BOOST_TEST(memory.size() % daily::VirtualMemory::page_size() == 0u);
}

BOOST_AUTO_TEST_CASE(VirtualMemory_regions_are_at_their_offsets) {
  daily::VirtualMemory const memory(128 * kGiB);
  auto low = memory.region(0, 64 * kGiB);
  auto high = memory.region(64 * kGiB, 64 * kGiB);
  BOOST_TEST(low.data() == memory.data());
  BOOST_TEST(high.data() - low.data() == static_cast<std::ptrdiff_t>(64 * kGiB));
  BOOST_TEST(high.size() == 64 * kGiB);
}

BOOST_AUTO_TEST_CASE(VirtualRegion_commit_keeps_contents) {
  std::size_t const page = daily::VirtualMemory::page_size();
  daily::VirtualMemory const memory(64 * kGiB);
  auto const region = memory.region();

  // Pages at both ends of the reservation can be committed separately.
  region.commit(0, page);
  region.commit(region.size() - page, page);
  region.data()[0] = std::byte{1};
  region.data()[region.size() - 1] = std::byte{2};

  // Committing again, here over a range including committed pages, keeps
  // what they hold.
  region.commit(0, 2 * page);
  BOOST_TEST(std::to_integer<int>(region.data()[0]) == 1);
  BOOST_TEST(std::to_integer<int>(region.data()[page]) == 0);
  BOOST_TEST(std::to_integer<int>(region.data()[region.size() - 1]) == 2);
}

BOOST_AUTO_TEST_CASE(VirtualRegion_decommit_discards_contents) {
  std::size_t const page = daily::VirtualMemory::page_size();
  daily::VirtualMemory const memory(4 * page);
  auto const region = memory.region();
  region.commit(0, 4 * page);
  std::fill_n(region.data(), 4 * page, std::byte{7});

  region.decommit(page, 2 * page);
  region.commit(page, 2 * page);
  BOOST_TEST(std::to_integer<int>(region.data()[page - 1]) == 7);
  BOOST_TEST(std::to_integer<int>(region.data()[page]) == 0);
  BOOST_TEST(std::to_integer<int>(region.data()[3 * page - 1]) == 0);
  BOOST_TEST(std::to_integer<int>(region.data()[3 * page]) == 7);
}

BOOST_AUTO_TEST_CASE(VirtualMemory_move_keeps_addresses) {
  std::size_t const page = daily::VirtualMemory::page_size();
  daily::VirtualMemory memory(4 * page);
  auto const region = memory.region(page, page);
  region.commit(0, page);
  region.data()[0] = std::byte{3};

  std::byte* const data = memory.data();
  daily::VirtualMemory moved(std::move(memory));
  BOOST_TEST(moved.data() == data);
  // NOLINTNEXTLINE(bugprone-use-after-move): checks the moved-from state
  BOOST_TEST(memory.data() == nullptr);
  BOOST_TEST(memory.size() == 0u);
  BOOST_TEST(std::to_integer<int>(region.data()[0]) == 3);

  daily::VirtualMemory assigned;
  assigned = std::move(moved);
  BOOST_TEST(assigned.data() == data);
  // NOLINTNEXTLINE(bugprone-use-after-move): checks the moved-from state
  BOOST_TEST(moved.data() == nullptr);
  BOOST_TEST(std::to_integer<int>(region.data()[0]) == 3);
}
