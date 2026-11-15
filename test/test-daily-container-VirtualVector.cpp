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
#include <stdexcept>
#include <string>
#include <utility>

#include "daily/container/VirtualVector.hpp"
#include "daily/memory/VirtualMemory.hpp"

namespace {

constexpr std::size_t kGiB = std::size_t(1) << 30;

// Counts live instances, and throws from the constructor once
// throw_after more have been made.
struct Counted {
  static inline int live = 0;
  static inline int throw_after = -1;

  Counted()
      : value(0) {
    if(throw_after == 0) {
      throw std::runtime_error("Counted");
    }
    if(throw_after > 0) {
      --throw_after;
    }
    ++live;
  }
  explicit Counted(int v)
      : value(v) {
    ++live;
  }
  Counted(Counted const& other)
      : value(other.value) {
    ++live;
  }
  ~Counted() {
    --live;
  }

  int value;
};

} // namespace

BOOST_AUTO_TEST_CASE(VirtualVector_default_is_empty) {
  daily::VirtualVector<int> v;
  BOOST_TEST(v.empty());
  BOOST_TEST(v.size() == 0u);
  BOOST_TEST(v.capacity() == 0u);
  BOOST_TEST(v.max_size() == 0u);
  BOOST_CHECK_THROW(v.push_back(1), std::length_error);
}

BOOST_AUTO_TEST_CASE(VirtualVector_starts_uncommitted) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<std::uint64_t> v(memory.region());
  BOOST_TEST(v.data() == reinterpret_cast<std::uint64_t*>(memory.data()));
  BOOST_TEST(v.capacity() == 0u);
  BOOST_TEST(v.max_size() == kGiB / sizeof(std::uint64_t));
}

// Growing past several commits leaves the elements where they were.
BOOST_AUTO_TEST_CASE(VirtualVector_grows_in_place) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<std::uint64_t> v(memory.region());
  v.push_back(0);
  std::uint64_t const* const first = &v[0];
  std::size_t const n = 4 * daily::VirtualVector<std::uint64_t>::kMinCommit;
  for(std::uint64_t i = 1; i < n; ++i) {
    v.push_back(i);
  }
  BOOST_TEST(&v[0] == first);
  BOOST_TEST(v.size() == n);
  BOOST_TEST(v.capacity() >= n);
  BOOST_TEST(v.capacity() < v.max_size());
  bool all = true;
  for(std::uint64_t i = 0; i < n; ++i) {
    all = all && v[i] == i;
  }
  BOOST_TEST(all);
  BOOST_TEST(v.front() == 0u);
  BOOST_TEST(v.back() == n - 1);
}

BOOST_AUTO_TEST_CASE(VirtualVector_push_back_own_element_while_growing) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<std::string> v(memory.region());
  v.push_back("first");
  while(v.size() < v.capacity()) {
    v.push_back("filler");
  }
  // This push commits more pages, with value referring into the vector.
  v.push_back(v.front());
  BOOST_TEST(v.back() == "first");
  BOOST_TEST(v.front() == "first");
}

// Vectors in regions at fixed offsets stay exactly that far apart.
BOOST_AUTO_TEST_CASE(VirtualVector_regions_keep_their_offset) {
  constexpr std::size_t kHalf = 64 * kGiB;
  daily::VirtualMemory memory(2 * kHalf);
  daily::VirtualVector<std::uint32_t> low(memory.region(0, kHalf));
  daily::VirtualVector<std::uint32_t> high(memory.region(kHalf, kHalf));
  for(std::uint32_t i = 0; i < 1'000'000; ++i) {
    low.push_back(i);
    high.push_back(i * 2);
  }
  auto const* base = reinterpret_cast<std::byte const*>(low.data());
  auto const* other = reinterpret_cast<std::byte const*>(high.data());
  BOOST_TEST(other - base == static_cast<std::ptrdiff_t>(kHalf));
  BOOST_TEST(low[999'999] == 999'999u);
  BOOST_TEST(high[999'999] == 1'999'998u);
}

BOOST_AUTO_TEST_CASE(VirtualVector_full_region_throws) {
  std::size_t const page = daily::VirtualMemory::page_size();
  daily::VirtualMemory memory(page);
  daily::VirtualVector<std::uint64_t> v(memory.region(0, page));
  v.resize(v.max_size());
  BOOST_TEST(v.capacity() == v.max_size());
  BOOST_CHECK_THROW(v.push_back(1), std::length_error);
  BOOST_CHECK_THROW(v.reserve(v.max_size() + 1), std::length_error);
  BOOST_TEST(v.size() == v.max_size());
}

BOOST_AUTO_TEST_CASE(VirtualVector_resize_constructs_and_destroys) {
  daily::VirtualMemory memory(kGiB);
  {
    daily::VirtualVector<Counted> v(memory.region());
    v.resize(10);
    BOOST_TEST(Counted::live == 10);
    BOOST_TEST(v[9].value == 0);

    v.resize(15, Counted(4));
    BOOST_TEST(Counted::live == 15);
    BOOST_TEST(v[9].value == 0);
    BOOST_TEST(v[14].value == 4);

    v.resize(5);
    BOOST_TEST(Counted::live == 5);
    v.pop_back();
    BOOST_TEST(Counted::live == 4);
    BOOST_TEST(v.size() == 4u);

    v.emplace_back(8);
    BOOST_TEST(v.back().value == 8);
    BOOST_TEST(Counted::live == 5);

    v.clear();
    BOOST_TEST(Counted::live == 0);
    BOOST_TEST(v.empty());
    BOOST_TEST(v.capacity() > 0u);

    v.resize(3);
  }
  BOOST_TEST(Counted::live == 0);
}

BOOST_AUTO_TEST_CASE(VirtualVector_resize_undoes_a_failed_construction) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<Counted> v(memory.region());
  v.resize(2);
  Counted::throw_after = 3;
  BOOST_CHECK_THROW(v.resize(10), std::runtime_error);
  Counted::throw_after = -1;
  BOOST_TEST(v.size() == 2u);
  BOOST_TEST(Counted::live == 2);
  v.clear();
}

BOOST_AUTO_TEST_CASE(VirtualVector_shrink_to_fit_decommits) {
  using Vector = daily::VirtualVector<std::uint64_t>;
  std::size_t const page = daily::VirtualMemory::page_size();
  daily::VirtualMemory memory(kGiB);
  Vector v(memory.region());
  v.resize(4 * Vector::kMinCommit / sizeof(std::uint64_t), 7);
  v[1] = 42;
  v.resize(page / sizeof(std::uint64_t) + 1);
  v.shrink_to_fit();
  BOOST_TEST(v.capacity() * sizeof(std::uint64_t) == 2 * page);
  BOOST_TEST(v[1] == 42u);

  // Pages committed again after shrinking start out zeroed.
  v.reserve(4 * page / sizeof(std::uint64_t));
  BOOST_TEST(v.data()[2 * page / sizeof(std::uint64_t)] == 0u);

  v.clear();
  v.shrink_to_fit();
  BOOST_TEST(v.capacity() == 0u);
  v.push_back(5);
  BOOST_TEST(v[0] == 5u);
}

BOOST_AUTO_TEST_CASE(VirtualVector_move_takes_the_region) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<std::string> a(memory.region());
  a.push_back("x");
  std::string const* const data = a.data();

  daily::VirtualVector<std::string> b(std::move(a));
  BOOST_TEST(b.data() == data);
  BOOST_TEST(b.size() == 1u);
  BOOST_TEST(a.empty());
  BOOST_TEST(a.max_size() == 0u);

  daily::VirtualVector<std::string> c;
  c = std::move(b);
  BOOST_TEST(c.data() == data);
  BOOST_TEST(c[0] == "x");
  BOOST_TEST(b.max_size() == 0u);

  swap(a, c);
  BOOST_TEST(a.data() == data);
  BOOST_TEST(c.max_size() == 0u);
}

// Destroying a vector decommits its pages, so another vector can start
// afresh in the same region.
BOOST_AUTO_TEST_CASE(VirtualVector_region_reused_after_destruction) {
  daily::VirtualMemory memory(kGiB);
  {
    daily::VirtualVector<std::uint64_t> v(memory.region());
    v.resize(1000, 9);
  }
  daily::VirtualVector<std::uint64_t> v(memory.region());
  BOOST_TEST(v.capacity() == 0u);
  v.reserve(1000);
  BOOST_TEST(v.data()[999] == 0u);
}

BOOST_AUTO_TEST_CASE(VirtualVector_iterates) {
  daily::VirtualMemory memory(kGiB);
  daily::VirtualVector<int> v(memory.region());
  for(int i = 0; i < 5; ++i) {
    v.push_back(i);
  }
  int sum = 0;
  for(int x : v) {
    sum += x;
  }
  BOOST_TEST(sum == 10);
  BOOST_TEST(*v.rbegin() == 4);
  BOOST_TEST(std::as_const(v).end() - std::as_const(v).begin() == 5);
}
