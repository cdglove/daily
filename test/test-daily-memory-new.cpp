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

// Tests for the replaced operator new and delete. Each test uses its own
// category, and checks values only after its scope ends, so allocations made
// by the test framework aren't charged to it.
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <thread>

#include "daily/memory/AllocationCategory.hpp"

namespace {

struct Plain {
  char data[1000];
};

} // namespace

#if DAILY_MEMORY_CATEGORIES

namespace {

struct alignas(64) Aligned {
  char data[100];
};

struct alignas(4096) PageAligned {
  char data[10];
};

std::int64_t heap(daily::AllocationCategory category) {
  return daily::allocation_category_usage(category).heap;
}

} // namespace

BOOST_AUTO_TEST_CASE(AllocationCategoryNew_charges_new_and_delete) {
  constexpr daily::AllocationCategory kCategory = 101;
  std::int64_t const before = heap(kCategory);
  std::int64_t during = 0;
  {
    daily::AllocationCategoryScope scope(kCategory);
    Plain* p = new Plain;
    during = heap(kCategory);
    delete p;
  }
  BOOST_TEST(during - before == static_cast<std::int64_t>(sizeof(Plain)));
  BOOST_TEST(heap(kCategory) == before);
}

BOOST_AUTO_TEST_CASE(AllocationCategoryNew_charges_arrays) {
  constexpr daily::AllocationCategory kCategory = 102;
  std::int64_t const before = heap(kCategory);
  std::int64_t during = 0;
  {
    daily::AllocationCategoryScope scope(kCategory);
    char* p = new char[777];
    during = heap(kCategory);
    delete[] p;
  }
  BOOST_TEST(during - before == 777);
  BOOST_TEST(heap(kCategory) == before);
}

BOOST_AUTO_TEST_CASE(AllocationCategoryNew_charges_nothrow_and_sized) {
  constexpr daily::AllocationCategory kCategory = 103;
  std::int64_t const before = heap(kCategory);
  std::int64_t after_nothrow = 0;
  std::int64_t after_sized = 0;
  bool allocated = false;
  {
    daily::AllocationCategoryScope scope(kCategory);
    Plain* p = new(std::nothrow) Plain;
    allocated = p != nullptr;
    after_nothrow = heap(kCategory);
    delete p;

    void* raw = ::operator new(500);
    after_sized = heap(kCategory);
    ::operator delete(raw, 500);
  }
  BOOST_TEST(allocated);
  BOOST_TEST(after_nothrow - before == static_cast<std::int64_t>(sizeof(Plain)));
  BOOST_TEST(after_sized - before == 500);
  BOOST_TEST(heap(kCategory) == before);
}

BOOST_AUTO_TEST_CASE(AllocationCategoryNew_aligns_over_aligned_types) {
  constexpr daily::AllocationCategory kCategory = 104;
  std::int64_t const before = heap(kCategory);
  std::int64_t during = 0;
  std::uintptr_t aligned_address = 0;
  std::uintptr_t page_aligned_address = 0;
  {
    daily::AllocationCategoryScope scope(kCategory);
    Aligned* a = new Aligned;
    PageAligned* b = new(std::nothrow) PageAligned;
    aligned_address = reinterpret_cast<std::uintptr_t>(a);
    page_aligned_address = reinterpret_cast<std::uintptr_t>(b);
    during = heap(kCategory);
    delete a;
    delete b;
  }
  BOOST_TEST(aligned_address % alignof(Aligned) == 0u);
  BOOST_TEST(page_aligned_address != 0u);
  BOOST_TEST(page_aligned_address % alignof(PageAligned) == 0u);
  BOOST_TEST(
      during - before ==
      static_cast<std::int64_t>(sizeof(Aligned) + sizeof(PageAligned)));
  BOOST_TEST(heap(kCategory) == before);
}

// The category travels with the allocation, so freeing on another thread,
// with no scope, still uncharges the right category.
BOOST_AUTO_TEST_CASE(AllocationCategoryNew_frees_on_another_thread) {
  constexpr daily::AllocationCategory kCategory = 105;
  std::int64_t const before = heap(kCategory);
  Plain* p = nullptr;
  {
    daily::AllocationCategoryScope scope(kCategory);
    p = new Plain;
  }
  BOOST_TEST(heap(kCategory) - before == static_cast<std::int64_t>(sizeof(Plain)));
  std::thread([p] { delete p; }).join();
  BOOST_TEST(heap(kCategory) == before);
}

#else

BOOST_AUTO_TEST_CASE(AllocationCategoryNew_disabled_is_a_no_op) {
  daily::AllocationCategoryScope scope(101);
  Plain* p = new Plain;
  BOOST_TEST(daily::allocation_category_usage(101).heap == 0);
  delete p;
}

#endif
