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
#include <chrono>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "daily/synchronisation/TicketGate.hpp"

namespace {

using namespace std::chrono_literals;

// Waits until done() is true, or the time is up, and says which.
template <typename Done>
bool wait_until(Done&& done, std::chrono::seconds limit = 10s) {
  auto const deadline = std::chrono::steady_clock::now() + limit;
  while(!done()) {
    if(std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

} // namespace

BOOST_AUTO_TEST_SUITE(daily_ticket_gate)

BOOST_AUTO_TEST_CASE(exclusive_requests_pass_alone) {
  daily::TicketGate gate;
  // Shared requests add 1, and exclusive ones 1000, while they're inside.
  std::atomic<int> inside = 0;
  std::atomic<int> overlaps = 0;
  std::atomic<int> passes = 0;
  auto const run = [&](bool shared) {
    int const weight = shared ? 1 : 1000;
    auto const acquire = [&] {
      int const others = inside.fetch_add(weight);
      if(shared ? others >= 1000 : others != 0) {
        ++overlaps;
      }
      ++passes;
      inside.fetch_sub(weight);
      return 0;
    };
    for(int i = 0; i != 2000; ++i) {
      if(shared) {
        gate.pass_shared(acquire);
      }
      else {
        gate.pass_exclusive(acquire);
      }
    }
  };
  std::vector<std::thread> threads;
  for(int t = 0; t != 4; ++t) {
    threads.emplace_back(run, t % 2 == 0);
  }
  for(auto& thread : threads) {
    thread.join();
  }
  BOOST_TEST(overlaps == 0);
  BOOST_TEST(passes == 8000);
}

BOOST_AUTO_TEST_CASE(shared_requests_pass_together) {
  daily::TicketGate gate;
  // Each waits inside acquire until the other is inside too, which can only
  // happen if they pass together.
  // Boost.Test's checks are only made on this thread.
  std::atomic<int> inside = 0;
  auto const acquire = [&] {
    ++inside;
    return wait_until([&] { return inside == 2; });
  };
  auto other = std::async(std::launch::async, [&] {
    return gate.pass_shared(acquire);
  });
  BOOST_TEST(gate.pass_shared(acquire));
  BOOST_TEST(other.get());
}

BOOST_AUTO_TEST_CASE(the_next_request_passes_after_a_throw) {
  daily::TicketGate gate;
  BOOST_CHECK_THROW(
      gate.pass_exclusive([]() -> int { throw std::runtime_error("failed"); }),
      std::runtime_error);
  BOOST_CHECK_THROW(
      gate.pass_shared([]() -> int { throw std::runtime_error("failed"); }),
      std::runtime_error);
  // If either throw had kept the gate shut, these would wait forever.
  BOOST_TEST(gate.pass_exclusive([] { return 1; }) == 1);
  BOOST_TEST(gate.pass_shared([] { return 2; }) == 2);
  BOOST_TEST(!gate.has_waiters());
}

BOOST_AUTO_TEST_CASE(has_waiters_while_a_request_is_held_up) {
  daily::TicketGate gate;
  std::shared_mutex mutex;
  BOOST_TEST(!gate.has_waiters());
  auto writer = gate.pass_exclusive([&] { return std::unique_lock(mutex); });
  BOOST_TEST(!gate.has_waiters());

  auto reader = std::async(std::launch::async, [&] {
    auto const lock = gate.pass_shared([&] { return std::shared_lock(mutex); });
  });
  BOOST_TEST(wait_until([&] { return gate.has_waiters(); }));
  writer.unlock();
  reader.get();
  BOOST_TEST(!gate.has_waiters());
}

// A thread that releases the lock and asks for it again goes behind a thread
// already waiting, whichever wakes first.
BOOST_AUTO_TEST_CASE(asking_again_goes_behind_those_waiting) {
  daily::TicketGate gate;
  std::shared_mutex mutex;
  auto writer = gate.pass_exclusive([&] { return std::unique_lock(mutex); });

  std::atomic<bool> read = false;
  auto reader = std::async(std::launch::async, [&] {
    auto const lock = gate.pass_shared([&] { return std::shared_lock(mutex); });
    read = true;
  });
  BOOST_REQUIRE(wait_until([&] { return gate.has_waiters(); }));

  writer.unlock();
  writer = gate.pass_exclusive([&] { return std::unique_lock(mutex); });
  // The reader got the lock before the writer had it back.
  BOOST_TEST(read);
  writer.unlock();
  reader.get();
}

BOOST_AUTO_TEST_SUITE_END()
