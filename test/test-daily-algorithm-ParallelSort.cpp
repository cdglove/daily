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
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "daily/algorithm/ParallelSort.hpp"

namespace {

class ThreadPool {
 public:
  explicit ThreadPool(unsigned count) {
    for(unsigned i = 0; i < count; ++i) {
      threads_.emplace_back([this] { work(); });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    ready_.notify_all();
    for(auto& thread : threads_) {
      thread.join();
    }
  }

  void submit(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push_back(std::move(task));
    }
    ready_.notify_one();
  }

 private:
  void work() {
    for(;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
        if(tasks_.empty()) {
          return;
        }
        task = std::move(tasks_.front());
        tasks_.pop_front();
      }
      task();
    }
  }

  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> tasks_;
  bool stopping_ = false;
  std::vector<std::thread> threads_;
};

struct InlineExecutor {
  template <typename Fn>
  void submit(Fn&& fn) {
    fn();
  }
};

struct RejectingExecutor {
  template <typename Fn>
  void submit(Fn&&) {
    throw std::runtime_error("rejected");
  }
};

std::vector<int> random_values(std::size_t count, std::mt19937& rng) {
  // A small range so there are plenty of duplicates.
  std::uniform_int_distribution<int> dist(0, 100);
  std::vector<int> values(count);
  for(auto& value : values) {
    value = dist(rng);
  }
  return values;
}

std::vector<std::string> random_strings(std::size_t count, std::mt19937& rng) {
  // Long enough to defeat the small string optimisation, so a lost move shows
  // up as an empty string.
  std::uniform_int_distribution<int> dist(0, 100);
  std::vector<std::string> values(count);
  for(auto& value : values) {
    value = std::to_string(dist(rng)) + std::string(32, 'x');
  }
  return values;
}

// Covers every combination of chunk count and odd/even merge rounds the
// algorithm can hit on this machine. A chunk size of one makes the small
// sizes take the parallel path.
std::size_t max_exhaustive_size() {
  return std::max(1u, std::thread::hardware_concurrency()) * 16;
}

template <typename Executor, typename Compare, typename MakeValues = decltype(&random_values)>
void check_sizes(Executor& executor, Compare compare, MakeValues make_values = &random_values) {
  std::mt19937 rng(42);
  std::vector<std::size_t> sizes;
  for(std::size_t size = 0; size <= max_exhaustive_size(); ++size) {
    sizes.push_back(size);
  }
  sizes.push_back(12345);
  sizes.push_back(100000);

  daily::detail::ParallelSorter<Executor> sorter(executor, 1);
  for(auto size : sizes) {
    auto values = make_values(size, rng);
    auto expected = values;
    std::sort(expected.begin(), expected.end(), compare);
    sorter(values.begin(), values.end(), compare);
    BOOST_TEST_INFO("size " << size);
    BOOST_TEST(values == expected, boost::test_tools::per_element());
  }
}

} // namespace

BOOST_AUTO_TEST_CASE(sorts_with_thread_pool) {
  ThreadPool pool(4);
  check_sizes(pool, std::less<>{});
  check_sizes(pool, std::greater<>{});
}

BOOST_AUTO_TEST_CASE(sorts_strings) {
  ThreadPool pool(4);
  check_sizes(pool, std::less<>{}, &random_strings);
}

BOOST_AUTO_TEST_CASE(sorts_with_default_chunk_size) {
  ThreadPool pool(4);
  std::mt19937 rng(42);
  for(std::size_t size : {0, 1, 8191, 8192, 8193, 100000, 1000000}) {
    auto values = random_values(size, rng);
    auto expected = values;
    std::sort(expected.begin(), expected.end());
    daily::parallel_sort(pool, values.begin(), values.end(), std::less<>{});
    BOOST_TEST_INFO("size " << size);
    BOOST_TEST(values == expected, boost::test_tools::per_element());
  }
}

BOOST_AUTO_TEST_CASE(sorts_with_single_thread_pool) {
  ThreadPool pool(1);
  check_sizes(pool, std::less<>{});
}

BOOST_AUTO_TEST_CASE(sorts_with_inline_executor) {
  InlineExecutor executor;
  check_sizes(executor, std::less<>{});
}

BOOST_AUTO_TEST_CASE(propagates_comparator_exception) {
  ThreadPool pool(4);
  std::mt19937 rng(42);
  auto values = random_values(10000, rng);
  std::atomic<int> calls{0};
  auto compare = [&calls](int a, int b) {
    if(++calls == 5000) {
      throw std::runtime_error("compare");
    }
    return a < b;
  };
  BOOST_CHECK_THROW(
      daily::parallel_sort(pool, values.begin(), values.end(), compare),
      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(sorts_with_rejecting_executor) {
  RejectingExecutor executor;
  check_sizes(executor, std::less<>{});
}

// The caller is the pool's only thread, so the sort only finishes if the
// caller runs the tasks it queued.
BOOST_AUTO_TEST_CASE(
    sorts_from_inside_single_thread_pool,
    *boost::unit_test::timeout(60)) {
  ThreadPool pool(1);
  std::mt19937 rng(42);
  auto values = random_values(100000, rng);
  auto expected = values;
  std::sort(expected.begin(), expected.end());

  std::promise<void> finished;
  pool.submit([&] {
    try {
      daily::parallel_sort(pool, values.begin(), values.end(), std::less<>{});
      finished.set_value();
    }
    catch(...) {
      finished.set_exception(std::current_exception());
    }
  });
  finished.get_future().get();
  BOOST_TEST(values == expected, boost::test_tools::per_element());
}
