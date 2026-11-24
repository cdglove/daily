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
#ifndef DAILY_SYNCHRONISATION_TICKETGATE_HPP
#define DAILY_SYNCHRONISATION_TICKETGATE_HPP
#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

namespace daily {

// Orders the acquisition of a reader-writer lock that isn't fair, such as
// boost::upgrade_mutex, by when each thread asked for it, so that a thread
// that releases the lock and asks for it again goes behind the threads
// already waiting, rather than taking it straight back before they wake.
//
// Readers pass shared requests and writers exclusive ones. A shared request
// only waits for the exclusive requests that came before it, so readers
// don't queue behind each other. An exclusive request waits for every
// request that came before it, of either kind, and the exclusive requests
// go through one at a time. Every request waits only for earlier ones, so
// none can wait for another that waits for it.
//
// Waiting threads sleep, on the address of the counter they're waiting for,
// rather than spin. Releasing a lock doesn't involve the gate.
//
// For example, with boost::upgrade_mutex:
//
//   daily::TicketGate gate;
//   boost::upgrade_mutex mutex;
//   auto read = gate.pass_shared([&] { return boost::shared_lock(mutex); });
//   auto write = gate.pass_exclusive([&] { return boost::unique_lock(mutex); });
//
// An upgrade lock is an exclusive request, as only one can be held at a
// time. Upgrading a lock that's held doesn't go through the gate, see
// pass_exclusive.
//
// A thread that keeps a lock for a long time can let the threads waiting go
// first by releasing it and asking again when has_waiters says someone is.
class TicketGate {
 public:
  TicketGate() = default;
  TicketGate(TicketGate const&) = delete;
  TicketGate& operator=(TicketGate const&) = delete;

  // Waits until the exclusive requests that came before have got through,
  // then returns what acquire returns.
  template <typename Acquire>
  auto pass_shared(Acquire&& acquire) {
    std::uint64_t const before = arrivals_.fetch_add(kShared);
    std::uint16_t const exclusive_before = exclusive_count(before);
    wait_until(exclusive_passed_, [&](std::uint16_t passed) {
      return passed == exclusive_before;
    });
    Passed const passed(shared_passed_);
    return std::forward<Acquire>(acquire)();
  }

  // Waits until every request that came before has got through, then
  // returns what acquire returns. Holding the gate while acquire waits for
  // the lock is what keeps the order, so a thread that already holds the
  // lock mustn't acquire it again through the gate, such as to upgrade it,
  // as it could wait behind a request that waits for it.
  template <typename Acquire>
  auto pass_exclusive(Acquire&& acquire) {
    std::uint64_t const before = arrivals_.fetch_add(kExclusive);
    std::uint16_t const ticket = exclusive_count(before);
    std::uint64_t const shared_before = shared_count(before);
    // The exclusive requests before it go through in order, after which no
    // other can until this one does.
    wait_until(exclusive_passed_, [&](std::uint16_t passed) {
      return passed == ticket;
    });
    // Shared requests that came after this one wait for it, so once as many
    // have passed as came before it, those are the ones that have.
    wait_until(shared_passed_, [&](std::uint64_t passed) {
      return static_cast<std::int64_t>(
                 ((passed - shared_before) << kExclusiveBits)) >= 0;
    });
    Passed const passed(exclusive_passed_);
    return std::forward<Acquire>(acquire)();
  }

  // Whether a request has arrived and not got through yet. The counters are
  // read one after another, so the answer can be out of date by the time
  // it's used. Passes are read before arrivals: a request arrives before it
  // passes, so this never counts more passes than arrivals.
  bool has_waiters() const {
    std::uint16_t const exclusive_passed = exclusive_passed_.load();
    std::uint64_t const shared_passed = shared_passed_.load();
    std::uint64_t const arrivals = arrivals_.load();
    return exclusive_count(arrivals) != exclusive_passed ||
           ((shared_count(arrivals) - shared_passed) & (kExclusive - 1)) != 0;
  }

 private:
  // A request's arrival adds to one part of arrivals_, so every request sees
  // how many of each kind came before it. Shared requests count in the low
  // bits, which take months of continuous locking to fill, so they never
  // carry into the exclusive count. That count wraps, which only drops a
  // bit off the top, and is compared modulo its width.
  static constexpr int kExclusiveBits = 16;
  static constexpr int kSharedBits = 64 - kExclusiveBits;
  static constexpr std::uint64_t kShared = 1;
  static constexpr std::uint64_t kExclusive = std::uint64_t{1} << kSharedBits;

  static std::uint16_t exclusive_count(std::uint64_t arrivals) {
    return static_cast<std::uint16_t>(arrivals >> kSharedBits);
  }
  static std::uint64_t shared_count(std::uint64_t arrivals) {
    return arrivals & (kExclusive - 1);
  }

  // Sleeps until done(counter) is true.
  template <typename T, typename Done>
  static void wait_until(std::atomic<T>& counter, Done&& done) {
    for(T value = counter.load(); !done(value); value = counter.load()) {
      counter.wait(value);
    }
  }

  // Counts a request as passed, and wakes those waiting for it, after
  // acquire, however it returns.
  template <typename T>
  class Passed {
   public:
    explicit Passed(std::atomic<T>& counter)
        : counter_(counter) {
    }
    Passed(Passed const&) = delete;
    Passed& operator=(Passed const&) = delete;
    ~Passed() {
      counter_.fetch_add(1);
      counter_.notify_all();
    }

   private:
    std::atomic<T>& counter_;
  };

  std::atomic<std::uint64_t> arrivals_{0};
  // The shared requests that have passed, which counts as shared_count does,
  // modulo 2 to the power kSharedBits.
  std::atomic<std::uint64_t> shared_passed_{0};
  std::atomic<std::uint16_t> exclusive_passed_{0};
};

} // namespace daily

#endif // DAILY_SYNCHRONISATION_TICKETGATE_HPP
