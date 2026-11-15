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
#include <boost/test/unit_test.hpp>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>

#include "daily/future/Future.hpp"

// Counts global heap allocations so tests can check that everything goes
// through the supplied allocator. Built as its own executable because it
// replaces operator new and delete.

static std::atomic<std::size_t> num_allocations(0);
static std::atomic<std::size_t> num_frees(0);
void* operator new(size_t size) {
  ++num_allocations;
  return malloc(size);
}
void* operator new[](size_t size) {
  ++num_allocations;
  return malloc(size);
}
void operator delete(void* ptr) noexcept {
  if(ptr) {
    ++num_frees;
    free(ptr);
  }
}
void operator delete[](void* ptr) noexcept {
  if(ptr) {
    ++num_frees;
    free(ptr);
  }
}
struct CheckAllocations {
  std::size_t count_;
  CheckAllocations()
      : count_(num_allocations) {
  }

  ~CheckAllocations() {
    BOOST_TEST_CHECK(count_ == num_allocations);
  }
};

namespace {

struct Buffer {
  Buffer()
      : head_(memory_) {
  }

  char memory_[10 * 1024 * 1024];
  char* head_;
};

template <class T>
struct LinearAllocator {
  using value_type = T;

  LinearAllocator()
      : buffer_(std::make_shared<Buffer>()) {
  }

  template <class U>
  LinearAllocator(LinearAllocator<U> const& other)
      : buffer_(other.buffer_) {
  }

  T* allocate(std::size_t n) {
    T* ret_val = reinterpret_cast<T*>(buffer_->head_);
    buffer_->head_ += ((sizeof(T) * n) + 7) & ~7;
    assert(buffer_->head_ <= std::end(buffer_->memory_));
    return ret_val;
  }
  void deallocate(T* p, std::size_t n) {
    // no-op
  }

  template <class U>
  struct rebind {
    using other = LinearAllocator<U>;
  };

 private:
  template <typename>
  friend struct LinearAllocator;

  std::shared_ptr<Buffer> buffer_;
};
} // namespace

template <typename Function, typename Allocator>
struct alloced_handler {
  using alloc_type = Allocator;

  alloced_handler(Function f, Allocator alloc)
      : function_(std::move(f))
      , allocator_(std::move(alloc)) {
  }

  template <typename... Args>
  auto operator()(Args... args) {
    return function_(std::forward<Args>(args)...);
  }

  Function function_;
  Allocator allocator_;
};

template <typename Function, typename Allocator>
alloced_handler<Function, Allocator> make_alloced_handler(Function&& f, Allocator a) {
  return alloced_handler<Function, typename std::decay<Allocator>::type>(
      std::forward<Function>(f),
      a);
}

float get_one() {
  return 1.f;
}

BOOST_AUTO_TEST_SUITE(daily_future_allocator)

BOOST_AUTO_TEST_CASE(future_alloc_promise) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  using T = int;
  daily::promise<T> p(std::allocator_arg, alloc);
  daily::future<T> f = p.get_future();
  p.set_value(1);
  BOOST_TEST_CHECK(f.valid() == true);
  BOOST_TEST_CHECK(f.get() == 1);
  BOOST_TEST_CHECK(f.valid() == false);
}

BOOST_AUTO_TEST_CASE(future_alloc_continuation_noncircular) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; }, alloc);
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); }, alloc);
  // Avoid a broken promise, which allocates its future_error.
  p.set_value(1.f);
}

BOOST_AUTO_TEST_CASE(future_alloc_continuation_void) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<void> p(std::allocator_arg, alloc);
  daily::future<void> f = p.get_future();
  daily::future<int> f2 = f.then([] { return 2; }, alloc);
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); }, alloc);
  // Avoid a broken promise, which allocates its future_error.
  p.set_value();
}

BOOST_AUTO_TEST_CASE(future_alloc_continuations_consistent) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  // The promises are satisfied because a broken promise allocates its
  // future_error with operator new.
  int ran = 0;
  daily::promise<char> pc(std::allocator_arg, alloc);
  auto fc = pc.get_future().then([&](char) { ++ran; }, alloc);
  daily::promise<void> pv(std::allocator_arg, alloc);
  auto fv = pv.get_future().then([&]() { ++ran; }, alloc);
  daily::promise<int&> pr(std::allocator_arg, alloc);
  auto fr = pr.get_future().then([&](int&) { ++ran; }, alloc);
  BOOST_TEST_CHECK(ran == 0);
  int value = 0;
  pc.set_value('c');
  pv.set_value();
  pr.set_value(value);
  BOOST_TEST_CHECK(ran == 3);
}

BOOST_AUTO_TEST_CASE(future_alloc_any_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; }, alloc);
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  daily::future<short> f3 = f2.then([](int i) { return (short)(i * 2); }, alloc);
  p.set_value(1.f);
  BOOST_TEST_CHECK(f3.get() == 4);
  f3 = daily::future<short>();
}

BOOST_AUTO_TEST_CASE(future_alloc_get_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; }, alloc);
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::get,
      [&continued](int i) {
        continued = true;
        return (short)(i * 2);
      },
      alloc);
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(continued == true);
}

BOOST_AUTO_TEST_CASE(future_alloc_get_chain_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 =
      f.then(daily::continue_on::get, [](float f) { return (int)f * 2; }, alloc);

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::get,
      [&continued](int i) {
        continued = true;
        return (short)(i * 2);
      },
      alloc);
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(continued == true);
}

BOOST_AUTO_TEST_CASE(future_alloc_set_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 = f.then([](float f) { return (int)f * 2; }, alloc);
  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::set,
      [&continued](int i) {
        continued = true;
        return (short)(i * 2);
      },
      alloc);
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == true);
  BOOST_TEST_CHECK(f3.get() == 4);
}

BOOST_AUTO_TEST_CASE(future_alloc_set_chain_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  daily::future<int> f2 =
      f.then(daily::continue_on::set, [](float f) { return (int)f * 2; }, alloc);

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool continued = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::set,
      [&continued](int i) {
        continued = true;
        return (short)(i * 2);
      },
      alloc);
  p.set_value(1.f);
  BOOST_TEST_CHECK(continued == true);
  BOOST_TEST_CHECK(f3.get() == 4);
}

BOOST_AUTO_TEST_CASE(future_alloc_get_set_chain_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  bool get_ran = false;
  daily::future<int> f2 = f.then(
      daily::continue_on::get,
      [&get_ran](float f) {
        get_ran = true;
        return (int)f * 2;
      },
      alloc);

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool set_ran = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::set,
      [&set_ran](int i) {
        set_ran = true;
        return (short)(i * 2);
      },
      alloc);
  BOOST_TEST_CHECK(get_ran == false);
  p.set_value(1.f);
  BOOST_TEST_CHECK(get_ran == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(get_ran == true);
  BOOST_TEST_CHECK(set_ran == true);
}

BOOST_AUTO_TEST_CASE(future_alloc_set_get_chain_continuation) {
  LinearAllocator<char> alloc;
  CheckAllocations check;
  daily::promise<float> p(std::allocator_arg, alloc);
  daily::future<float> f = p.get_future();
  bool set_ran = false;
  daily::future<int> f2 = f.then(
      daily::continue_on::set,
      [&set_ran](float f) {
        set_ran = true;
        return (int)f * 2;
      },
      alloc);

  BOOST_TEST_CHECK(f2.valid() == true);
  BOOST_TEST_CHECK(f.valid() == false);
  bool get_ran = false;
  daily::future<short> f3 = f2.then(
      daily::continue_on::get,
      [&get_ran](int i) {
        get_ran = true;
        return (short)(i * 2);
      },
      alloc);
  BOOST_TEST_CHECK(set_ran == false);
  p.set_value(1.f);
  BOOST_TEST_CHECK(set_ran == true);
  BOOST_TEST_CHECK(get_ran == false);
  BOOST_TEST_CHECK(f3.get() == 4);
  BOOST_TEST_CHECK(get_ran == true);
}

BOOST_AUTO_TEST_SUITE_END()
