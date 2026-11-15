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
#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

#include "daily/future/Future.hpp"

namespace {
// Each specialisation of future and promise.
using AllResultTypes = boost::mpl::list<int, int&, void>;
} // namespace

BOOST_AUTO_TEST_SUITE(daily_future)

BOOST_AUTO_TEST_CASE_TEMPLATE(future_initial_state, T, AllResultTypes) {
  daily::future<T> f;
  BOOST_TEST_CHECK(f.valid() == false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(promise_initial_state, T, AllResultTypes) {
  daily::promise<T> p;
  auto f = p.get_future();
  BOOST_TEST_CHECK(f.valid() == true);
}

BOOST_AUTO_TEST_CASE(promise_future_communication) {
  {
    using T = int;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    p.set_value(1);
    BOOST_TEST_CHECK(f.valid() == true);
    BOOST_TEST_CHECK(f.get() == 1);
    BOOST_TEST_CHECK(f.valid() == false);
  }

  {
    using T = int&;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    int result = 1;
    p.set_value(result);
    BOOST_TEST_CHECK(f.valid() == true);
    BOOST_TEST_CHECK(&f.get() == &result);
    BOOST_TEST_CHECK(f.valid() == false);
  }

  {
    using T = void;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    p.set_value();
    BOOST_TEST_CHECK(f.valid() == true);
    f.get();
    BOOST_TEST_CHECK(f.valid() == false);
  }
}

BOOST_AUTO_TEST_CASE(promise_move_semantics) {
  {
    using T = int;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::promise<T> p2 = std::move(p);
    p2.set_value(1);
    BOOST_TEST_CHECK(f.valid() == true);
    BOOST_TEST_CHECK(f.get() == 1);
    BOOST_TEST_CHECK(f.valid() == false);
  }

  {
    using T = int&;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::promise<T> p2 = std::move(p);
    int result = 1;
    p2.set_value(result);
    BOOST_TEST_CHECK(f.valid() == true);
    BOOST_TEST_CHECK(&f.get() == &result);
    BOOST_TEST_CHECK(f.valid() == false);
  }

  {
    using T = void;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::promise<T> p2 = std::move(p);
    p2.set_value();
    BOOST_TEST_CHECK(f.valid() == true);
    f.get();
    BOOST_TEST_CHECK(f.valid() == false);
  }
}

BOOST_AUTO_TEST_CASE(future_move_semantics) {
  {
    using T = int;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::future<T> f2 = std::move(f);
    BOOST_TEST_CHECK(f2.valid() == true);
    BOOST_TEST_CHECK(f.valid() == false);
    p.set_value(3);
    BOOST_TEST_CHECK(f2.get() == 3);
    BOOST_TEST_CHECK(f2.valid() == false);
  }

  {
    using T = int&;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::future<T> f2 = std::move(f);
    BOOST_TEST_CHECK(f2.valid() == true);
    BOOST_TEST_CHECK(f.valid() == false);
    int result = 3;
    p.set_value(result);
    BOOST_TEST_CHECK(f2.get() == 3);
    BOOST_TEST_CHECK(f2.valid() == false);
  }

  {
    using T = void;
    daily::promise<T> p;
    daily::future<T> f = p.get_future();
    daily::future<T> f2 = std::move(f);
    BOOST_TEST_CHECK(f2.valid() == true);
    BOOST_TEST_CHECK(f.valid() == false);
    p.set_value();
    f2.get();
    BOOST_TEST_CHECK(f2.valid() == false);
  }
}

BOOST_AUTO_TEST_CASE(future_continuation_noncircular) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); });
}

BOOST_AUTO_TEST_CASE(future_continuation_void) {
  daily::promise<void> p;
  daily::future<void> f = p.get_future();
  daily::future<int> f2 = f.then([] { return 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); });
}

BOOST_AUTO_TEST_CASE(future_continuations_consistent) {
  bool ran = false;
  daily::promise<char>().get_future().then([&](char) { ran = true; });
  BOOST_TEST_CHECK(!ran);
  daily::promise<void>().get_future().then([&]() { ran = true; });
  BOOST_TEST_CHECK(!ran);
  daily::promise<int&>().get_future().then([&](int&) { ran = true; });
  BOOST_TEST_CHECK(!ran);
}

BOOST_AUTO_TEST_CASE(future_any_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); });
  p.set_value(1.f);
  BOOST_TEST_CHECK(f3.get() == 4);
  f3 = daily::future<short>();
}

BOOST_AUTO_TEST_CASE(future_get_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(daily::continue_on::get, [&continued](int i) {
    continued = true;
    return (short)(i * 2);
  });
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(continued == true);
}

BOOST_AUTO_TEST_CASE(future_get_chain_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then(daily::continue_on::get, [](float f) {
    return (int)f * 2;
  });

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(daily::continue_on::get, [&continued](int i) {
    continued = true;
    return (short)(i * 2);
  });
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(continued == true);
}

BOOST_AUTO_TEST_CASE(future_set_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(daily::continue_on::set, [&continued](int i) {
    continued = true;
    return (short)(i * 2);
  });
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == true);
  BOOST_TEST_CHECK(f3.get() == 4);
}

BOOST_AUTO_TEST_CASE(future_set_chain_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then(daily::continue_on::set, [](float f) {
    return (int)f * 2;
  });

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(daily::continue_on::set, [&continued](int i) {
    continued = true;
    return (short)(i * 2);
  });
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == true);
  BOOST_TEST_CHECK(f3.get() == 4);
}

BOOST_AUTO_TEST_CASE(future_get_set_chain_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  bool get_ran = false;
  daily::future<int> f2 = f.then(daily::continue_on::get, [&get_ran](float f) {
    get_ran = true;
    return (int)f * 2;
  });

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool set_ran = false;
  daily::future<short> f3 = f2.then(daily::continue_on::set, [&set_ran](int i) {
    set_ran = true;
    return (short)(i * 2);
  });
  BOOST_TEST_CHECK(get_ran == false);
  p.set_value(1.f);
  BOOST_TEST_CHECK(get_ran == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(get_ran == true);
  BOOST_TEST_CHECK(set_ran == true);
}

BOOST_AUTO_TEST_CASE(future_set_get_chain_continuation) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  bool set_ran = false;
  daily::future<int> f2 = f.then(daily::continue_on::set, [&set_ran](float f) {
    set_ran = true;
    return (int)f * 2;
  });

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool get_ran = false;
  daily::future<short> f3 = f2.then(daily::continue_on::get, [&get_ran](int i) {
    get_ran = true;
    return (short)(i * 2);
  });
  BOOST_TEST_CHECK(set_ran == false);
  p.set_value(1.f);
  BOOST_TEST_CHECK(set_ran == true);
  BOOST_TEST_CHECK(get_ran == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(get_ran == true);
}

BOOST_AUTO_TEST_CASE(future_multithread) {
  int repeat = 100;
  while(repeat--) {
    daily::promise<float> p;
    daily::future<float> f = p.get_future();
    std::jthread t([&p, repeat] { p.set_value((float)repeat); });
    BOOST_TEST_CHECK(f.get() == repeat);
  }
}

BOOST_AUTO_TEST_CASE(future_multithread_get_continuation) {
  int repeat = 100;
  while(repeat--) {
    daily::promise<int> p;
    daily::future<int> f = p.get_future();
    std::jthread t([&p, repeat] { p.set_value(repeat); });
    auto f2 = f.then(daily::continue_on::get, [](int v) { return v * 2; });
    BOOST_TEST_CHECK(f2.get() == (2 * repeat));
  }
}

BOOST_AUTO_TEST_CASE(promise_future_get_throws) {
  // We don't need to test each specialization here because
  // the execption mechanism is in the common implementation.
  using T = int;
  daily::promise<T> p;
  daily::future<T> f = p.get_future();

  try {
    throw std::logic_error("");
  }
  catch(std::logic_error&) {
    p.set_exception(std::current_exception());
  }

  BOOST_TEST_CHECK(f.valid() == true);

  bool exception_caught = false;
  try {
    f.get();
  }
  catch(std::logic_error&) {
    exception_caught = true;
  }
  BOOST_TEST_CHECK(exception_caught == true);
  BOOST_TEST_CHECK(f.valid() == false);
}

BOOST_AUTO_TEST_CASE(promise_future_get_continuation_throws) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; });
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then(daily::continue_on::get, [](int i) {
    throw std::logic_error("");
    return (short)(i * 2);
  });
  p.set_value(1.f);

  bool exception_caught = false;
  try {
    f3.get();
  }
  catch(std::logic_error&) {
    exception_caught = true;
  }
  BOOST_TEST_CHECK(exception_caught == true);
}

BOOST_AUTO_TEST_CASE(promise_future_set_continuation_throws) {
  daily::promise<float> p;
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then(daily::continue_on::set, [](float f) {
    throw std::logic_error("");
    return (int)f * 2;
  });

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then(daily::continue_on::get, [](int i) {
    return (short)(i * 2);
  });

  bool exception_caught = false;
  try {
    p.set_value(1.f);
  }
  catch(std::logic_error&) {
    exception_caught = true;
  }
  BOOST_TEST_CHECK(exception_caught == true);
}

BOOST_AUTO_TEST_CASE(future_wait) {
  daily::promise<int> promise;
  daily::future<int> future = promise.get_future();
  std::thread run_delayed([&] { promise.set_value(5); });
  BOOST_TEST_CHECK(5 == future.get());
  run_delayed.join();
}

// The following tests were lifted from
// https://github.com/skarupke/compile_time/blob/master/await/then_future.cpp
// as a few edge cases I missed.
BOOST_AUTO_TEST_CASE(discard_future) {
  daily::promise<void> promise;
  bool ran = false;
  promise.get_future().then([&ran]() { ran = true; });
  promise.set_value();
  BOOST_TEST_CHECK(ran == true);
}

BOOST_AUTO_TEST_CASE(discard_promise) {
  daily::future<void> future = daily::promise<void>().get_future();
  bool thrown = false;
  try {
    future.get();
  }
  catch(const daily::future_error& error) {
    thrown = true;
    BOOST_TEST_CHECK((int)daily::future_errc::broken_promise == (int)error.code());
  }
  BOOST_TEST_CHECK(thrown == true);
}

BOOST_AUTO_TEST_CASE(discard_both) {
  daily::promise<void>().get_future().then([]() {});
}

BOOST_AUTO_TEST_CASE(set_before_get) {
  daily::promise<void> promise;
  promise.set_value();
  bool ran = false;
  promise.get_future().then([&ran]() { ran = true; });
  BOOST_TEST_CHECK(ran == true);
}

struct MovableFunctor {
  MovableFunctor()
      : a(new int(5)) {
  }

  int operator()() {
    return *a;
  }

  std::unique_ptr<int> a;
};

// this test is here mainly to ensure that the code
// compiles with functors that are movable only
BOOST_AUTO_TEST_CASE(daily_future_movable) {
  daily::promise<void> promise;
  daily::future<int> future = promise.get_future().then(MovableFunctor());
  promise.set_value();
  BOOST_TEST_CHECK(5 == future.get());
}

BOOST_AUTO_TEST_CASE(moved_promise_destroyed_breaks_promise) {
  daily::future<int> f;
  {
    daily::promise<int> p;
    f = p.get_future();
    daily::promise<int> p2 = std::move(p);
  }
  BOOST_CHECK_THROW(f.get(), daily::future_error);
}

BOOST_AUTO_TEST_CASE(promise_assigned_over_breaks_promise) {
  daily::promise<int> p;
  daily::future<int> f = p.get_future();
  p = daily::promise<int>();
  BOOST_CHECK_THROW(f.get(), daily::future_error);
}

BOOST_AUTO_TEST_CASE(swapped_promise_sets_value) {
  daily::promise<int> p;
  daily::promise<int> p2;
  daily::future<int> f = p.get_future();
  p.swap(p2);
  p2.set_value(4);
  BOOST_TEST_CHECK(f.get() == 4);
}

BOOST_AUTO_TEST_CASE(set_value_twice_throws) {
  daily::promise<int> p;
  p.set_value(1);
  BOOST_CHECK_THROW(p.set_value(2), daily::future_error);
}

BOOST_AUTO_TEST_CASE(get_future_twice_throws) {
  daily::promise<int> p;
  auto f = p.get_future();
  BOOST_CHECK_THROW(p.get_future(), daily::future_error);
}

BOOST_AUTO_TEST_CASE(exception_propagates_through_any_continuation) {
  daily::promise<int> p;
  daily::future<int> f = p.get_future().then([](int i) { return i; });
  p.set_exception(std::make_exception_ptr(std::runtime_error("")));
  BOOST_CHECK_THROW(f.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(exception_propagates_through_get_continuation) {
  daily::promise<int> p;
  daily::future<int> f = p.get_future().then(daily::continue_on::get, [](int i) {
    return i;
  });
  p.set_exception(std::make_exception_ptr(std::runtime_error("")));
  BOOST_CHECK_THROW(f.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(exception_propagates_through_set_continuation) {
  daily::promise<int> p;
  daily::future<int> f = p.get_future().then(daily::continue_on::set, [](int i) {
    return i;
  });
  p.set_exception(std::make_exception_ptr(std::runtime_error("")));
  BOOST_CHECK_THROW(f.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(broken_promise_propagates_through_continuation) {
  daily::future<int> f = daily::promise<int>().get_future().then(
      [](int i) { return i; });
  BOOST_CHECK_THROW(f.get(), daily::future_error);
}

BOOST_AUTO_TEST_CASE(wait_for_times_out) {
  daily::promise<int> p;
  daily::future<int> f = p.get_future();
  BOOST_TEST_CHECK(
      (f.wait_for(std::chrono::milliseconds(1)) == daily::future_status::timeout));
  p.set_value(1);
  BOOST_TEST_CHECK(
      (f.wait_for(std::chrono::milliseconds(1)) == daily::future_status::ready));
}

BOOST_AUTO_TEST_SUITE_END()
