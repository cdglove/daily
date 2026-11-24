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
#include <atomic>
#include <chrono>
#include <boost/asio/async_result.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <utility>

#include "daily/future/UseFuture.hpp"

namespace {

float get_one() {
  return 1.f;
}

// An asynchronous operation that completes with get_one() on ex.
template <typename Executor, typename CompletionToken>
auto async_get_one(Executor ex, CompletionToken&& token) {
  return boost::asio::async_initiate<CompletionToken, void(float)>(
      [](auto handler, Executor const& ex) {
        boost::asio::dispatch(ex, [handler = std::move(handler)]() mutable {
          std::move(handler)(get_one());
        });
      },
      token,
      std::move(ex));
}

} // namespace

BOOST_AUTO_TEST_SUITE(daily_use_future)

BOOST_AUTO_TEST_CASE(future_use_future_void) {
  boost::asio::io_context io;
  daily::future<void> f = boost::asio::post(io, daily::use_future);
  BOOST_TEST_CHECK(f.valid() == true);
  io.run();
  f.get();
  BOOST_TEST_CHECK(f.valid() == false);
}

BOOST_AUTO_TEST_CASE(future_use_future_basic) {
  boost::asio::thread_pool pool;
  auto f = async_get_one(pool.get_executor(), daily::use_future);

  BOOST_TEST_CHECK(f.valid() == true);
  auto f2 = f.then(daily::continue_on::get, [](float f) { return f * 2; });
  BOOST_TEST_CHECK(f.valid() == false);
  BOOST_TEST_CHECK(f2.get() == 2.f);
}

BOOST_AUTO_TEST_CASE(future_use_future_throw) {
  boost::asio::thread_pool pool;
  auto f = async_get_one(pool.get_executor(), daily::use_future);

  BOOST_TEST_CHECK(f.valid() == true);
  auto f2 = f.then(daily::continue_on::get, [](float f) {
    throw std::logic_error("");
    return f * 2;
  });
  BOOST_TEST_CHECK(f.valid() == false);

  bool exception_caught = false;
  try {
    f2.get();
  }
  catch(std::logic_error&) {
    exception_caught = true;
  }
  BOOST_TEST_CHECK(exception_caught == true);
  BOOST_TEST_CHECK(f2.valid() == false);
}

BOOST_AUTO_TEST_CASE(future_use_future_executor) {
  boost::asio::thread_pool pool;
  boost::asio::io_context looper;
  auto f = async_get_one(pool.get_executor(), daily::use_future);

  BOOST_TEST_CHECK(f.valid() == true);
  bool has_run = false;
  f.wait();
  auto f2 = f.then(daily::execute::dispatch, looper, [&has_run](float f) {
    has_run = true;
    return f * 2.f;
  });

  BOOST_TEST_CHECK(f.valid() == false);
  BOOST_TEST_CHECK(has_run == false);
  looper.run();
  BOOST_TEST_CHECK(has_run == true);
  BOOST_TEST_CHECK(f2.get() == 2.f);
}

// A continuation that throws on the executor gives its future the exception,
// rather than throwing it into the executor and leaving the future
// unfinished.
BOOST_AUTO_TEST_CASE(continuation_that_throws_on_an_executor) {
  boost::asio::io_context looper;
  daily::promise<float> p;
  auto f = p.get_future().then(daily::execute::post, looper, [](float) -> float {
    throw std::runtime_error("on the executor");
  });
  p.set_value(1.f);
  BOOST_CHECK_NO_THROW(looper.run());
  BOOST_TEST_REQUIRE((f.wait_for(std::chrono::seconds(10)) == daily::future_status::ready));
  BOOST_CHECK_THROW(f.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(future_use_future_stress) {
  boost::asio::thread_pool pool;
  int count = 10000;
  std::atomic<float> result(10000);
  while(count--) {
    auto f = async_get_one(pool.get_executor(), daily::use_future);

    f.then(daily::execute::dispatch, pool, [&result](float f) {
      auto current = result.load();
      while(!result.compare_exchange_weak(current, current - f)) {
        ;
      }
    });
  }

  pool.join();
  BOOST_TEST_CHECK(result.load() == 0.f);
}

BOOST_AUTO_TEST_CASE(future_execute_post_and_defer) {
  boost::asio::io_context looper;
  daily::promise<int> p1;
  daily::promise<int> p2;
  auto f1 = p1.get_future().then(daily::execute::post, looper, [](int i) {
    return i + 1;
  });
  auto f2 = p2.get_future().then(daily::execute::defer, looper, [](int i) {
    return i + 2;
  });
  p1.set_value(1);
  p2.set_value(1);
  looper.run();
  BOOST_TEST_CHECK(f1.get() == 2);
  BOOST_TEST_CHECK(f2.get() == 3);
}

BOOST_AUTO_TEST_SUITE_END()
