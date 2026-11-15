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
#include <memory>
#include <stdexcept>
#include <utility>

#include "daily/future/Future.hpp"

BOOST_AUTO_TEST_SUITE(daily_packaged_task)

BOOST_AUTO_TEST_CASE(call_sets_future) {
  daily::packaged_task<int(int)> pt([](int i) { return i * 2; });
  daily::future<int> f = pt.get_future();
  pt(5);
  BOOST_TEST_CHECK(f.get() == 10);
}

BOOST_AUTO_TEST_CASE(void_result) {
  bool ran = false;
  daily::packaged_task<void()> pt([&ran] { ran = true; });
  daily::future<void> f = pt.get_future();
  pt();
  BOOST_TEST_CHECK(ran);
  f.get();
}

BOOST_AUTO_TEST_CASE(exception_is_stored) {
  daily::packaged_task<int()> pt([]() -> int { throw std::runtime_error("x"); });
  daily::future<int> f = pt.get_future();
  pt();
  BOOST_CHECK_THROW(f.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(valid) {
  daily::packaged_task<int()> empty;
  BOOST_TEST_CHECK(!empty.valid());
  daily::packaged_task<int()> pt([] { return 1; });
  BOOST_TEST_CHECK(pt.valid());
}

BOOST_AUTO_TEST_CASE(move_and_swap) {
  daily::packaged_task<int()> a([] { return 1; });
  daily::packaged_task<int()> b([] { return 2; });
  daily::future<int> fa = a.get_future();
  daily::future<int> fb = b.get_future();
  a.swap(b);
  daily::packaged_task<int()> c;
  c = std::move(a);
  c();
  b();
  BOOST_TEST_CHECK(fa.get() == 1);
  BOOST_TEST_CHECK(fb.get() == 2);
}

BOOST_AUTO_TEST_CASE(reset_allows_another_call) {
  int calls = 0;
  daily::packaged_task<int()> pt([&calls] { return ++calls; });
  daily::future<int> f1 = pt.get_future();
  pt();
  BOOST_TEST_CHECK(f1.get() == 1);
  pt.reset();
  daily::future<int> f2 = pt.get_future();
  pt();
  BOOST_TEST_CHECK(f2.get() == 2);
}

BOOST_AUTO_TEST_CASE(allocator_construction) {
  std::allocator<char> alloc;
  daily::packaged_task<int()> pt(std::allocator_arg, alloc, [] { return 3; });
  daily::future<int> f = pt.get_future();
  pt();
  BOOST_TEST_CHECK(f.get() == 3);
}

BOOST_AUTO_TEST_CASE(destroyed_without_call_breaks_promise) {
  daily::future<int> f;
  {
    daily::packaged_task<int()> pt([] { return 1; });
    f = pt.get_future();
  }
  BOOST_CHECK_THROW(f.get(), daily::future_error);
}

BOOST_AUTO_TEST_SUITE_END()
