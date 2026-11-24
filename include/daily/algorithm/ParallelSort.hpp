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
#ifndef DAILY_ALGORITHM_PARALLELSORT_HPP
#define DAILY_ALGORITHM_PARALLELSORT_HPP
#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace daily {

namespace detail {

// Tracks a batch of tasks so the caller can block until they have all
// finished. Tasks are queued here and the executor is only given a runner that
// pops the next one. While waiting, the caller drains the queue itself, so
// the batch completes even if the executor never gets to the runners. This
// makes it safe to use from one of the executor's own threads. The first
// exception thrown by a task is rethrown from wait().
class TaskGroup {
 public:
  // If the executor rejects the runner, the task stays queued and runs on the
  // calling thread in wait().
  template <typename Executor, typename Fn>
  void submit(Executor& executor, Fn fn) {
    {
      std::scoped_lock lock(state_->mutex);
      state_->queue.emplace_back(std::move(fn));
      ++state_->pending;
    }

    try {
      executor.submit([state = state_] { state->run_next(); });
    }
    // NOLINTNEXTLINE(bugprone-empty-catch): left in the queue for wait()
    catch(...) {
    }
  }

  // Runs fn on the calling thread, capturing any exception for wait().
  template <typename Fn>
  void run(Fn&& fn) noexcept {
    state_->run(fn);
  }

  void wait() {
    while(state_->run_next()) {
    }

    // Anything still pending is already running on another thread.
    std::unique_lock<std::mutex> lock(state_->mutex);
    state_->done.wait(lock, [this] { return state_->pending == 0; });
    if(state_->error) {
      std::rethrow_exception(std::exchange(state_->error, nullptr));
    }
  }

 private:
  // Shared with the runners because they can start after the batch is done,
  // find the queue empty, and return.
  struct State {
    bool run_next() {
      std::function<void()> task;
      {
        std::scoped_lock const lock(mutex);
        if(queue.empty()) {
          return false;
        }
        task = std::move(queue.front());
        queue.pop_front();
      }

      run(task);

      std::scoped_lock lock(mutex);
      if(--pending == 0) {
        done.notify_all();
      }
      return true;
    }

    template <typename Fn>
    void run(Fn& fn) noexcept {
      try {
        fn();
      }
      catch(...) {
        std::scoped_lock lock(mutex);
        if(!error) {
          error = std::current_exception();
        }
      }
    }

    std::mutex mutex;
    std::condition_variable done;
    std::deque<std::function<void()>> queue;
    // Tasks queued or running.
    std::size_t pending = 0;
    std::exception_ptr error;
  };

  std::shared_ptr<State> state_ = std::make_shared<State>();
};

// Returns how many of the first i elements of a stable merge of a and b come
// from a. Elements of a go first when equivalent, matching std::merge.
template <typename RandomIt, typename Difference, typename Compare>
Difference merge_split(
    RandomIt a,
    Difference a_size,
    RandomIt b,
    Difference b_size,
    Difference i,
    Compare& compare) {
  Difference low = std::max<Difference>(0, i - b_size);
  Difference high = std::min(i, a_size);
  while(low < high) {
    Difference mid = low + (high - low) / 2;
    if(compare(b[i - mid - 1], a[mid])) {
      high = mid;
    }
    else {
      low = mid + 1;
    }
  }
  return low;
}

// Sorts chunks of the range in parallel, then merges adjacent pairs of sorted
// runs until one is left. Each merge is split into pieces using merge_split,
// so every round keeps all the threads busy, not just the early ones. Merges
// are out of place, moving the data between the range and a scratch buffer.
template <typename Executor>
class ParallelSorter {
 public:
  // Ranges are split into chunks of at least min_chunk_size elements, so ranges
  // shorter than twice that are sorted on the calling thread.
  explicit ParallelSorter(Executor& workers, std::size_t min_chunk_size = 4096)
      : workers_(workers)
      , min_chunk_size_(std::max<std::size_t>(1, min_chunk_size)) {
  }

  // Blocks until the range is sorted. The calling thread takes part in the
  // work, so this may be called from one of the executor's own threads. The
  // value type must be default constructible and move assignable. If compare
  // throws, the range is left in a valid but unspecified state.
  template <typename Iterator, typename Sentinel, typename Compare>
  void operator()(Iterator begin, Sentinel end, Compare&& compare) {
    using Difference = std::iter_difference_t<Iterator>;
    using Value = std::iter_value_t<Iterator>;
    static_assert(
        std::is_default_constructible_v<Value>,
        "parallel_sort needs a default constructible value type for its "
        "scratch buffer.");

    Iterator last = std::ranges::next(begin, end);
    Difference const size = last - begin;

    // hardware_concurrency() returns 0 when it can't be determined.
    auto const threads = static_cast<Difference>(
        std::max(1u, std::thread::hardware_concurrency()));
    Difference const num_chunks = std::min(
        threads,
        size / static_cast<Difference>(min_chunk_size_));
    if(num_chunks < 2) {
      std::sort(begin, last, compare);
      return;
    }

    // Boundaries of the sorted runs, starting with one run per chunk.
    std::vector<Difference> runs(static_cast<std::size_t>(num_chunks) + 1);
    for(Difference i = 0; i <= num_chunks; ++i) {
      runs[static_cast<std::size_t>(i)] = size * i / num_chunks;
    }

    int rounds = 0;
    for(Difference n = num_chunks; n > 1; n = (n + 1) / 2) {
      ++rounds;
    }

    // Each round moves the data to the other side, so with an odd number of
    // rounds the chunks are sorted in the buffer and the last round lands back
    // in the range.
    std::vector<Value> buffer(static_cast<std::size_t>(size));
    bool in_buffer = rounds % 2 == 1;
    TaskGroup tasks;

    for(Difference i = 0; i < num_chunks; ++i) {
      Difference const chunk_begin = runs[static_cast<std::size_t>(i)];
      Difference const chunk_end = runs[static_cast<std::size_t>(i) + 1];
      tasks.submit(workers_, [=, &buffer, &compare] {
        if(in_buffer) {
          auto out = buffer.begin() + chunk_begin;
          std::move(begin + chunk_begin, begin + chunk_end, out);
          std::sort(out, buffer.begin() + chunk_end, compare);
        }
        else {
          std::sort(begin + chunk_begin, begin + chunk_end, compare);
        }
      });
    }

    tasks.wait();

    while(runs.size() > 2) {
      if(in_buffer) {
        merge_round(buffer.begin(), begin, runs, size, threads, compare, tasks);
      }
      else {
        merge_round(begin, buffer.begin(), runs, size, threads, compare, tasks);
      }

      in_buffer = !in_buffer;
    }
  }

 private:
  // Merges adjacent pairs of runs from source into destination and replaces
  // runs with the boundaries of the merged runs.
  template <typename Source, typename Destination, typename Difference, typename Compare>
  void merge_round(
      Source source,
      Destination destination,
      std::vector<Difference>& runs,
      Difference size,
      Difference threads,
      Compare& compare,
      TaskGroup& tasks) {
    // Split each merge in proportion to its share of the range so the round
    // has about this many equally sized pieces in total.
    Difference const round_pieces = threads * 2;
    auto const pieces_for = [&](Difference count) {
      return std::clamp<Difference>((count * round_pieces + size - 1) / size, 1, count);
    };

    std::vector<Difference> merged;
    merged.reserve(runs.size() / 2 + 2);
    std::vector<Difference> splits;

    std::size_t run = 0;
    for(; run + 2 < runs.size(); run += 2) {
      Difference first = runs[run];
      Difference middle = runs[run + 1];
      Difference last = runs[run + 2];
      Difference count = last - first;
      Difference pieces = pieces_for(count);
      auto const a = source + first;
      auto const b = source + middle;
      Difference a_size = middle - first;
      Difference b_size = last - middle;

      // Find every split before submitting any piece. The searches read
      // anywhere in both runs, and once a piece starts it moves elements out.
      splits.resize(static_cast<std::size_t>(pieces) + 1);
      for(Difference piece = 0; piece <= pieces; ++piece) {
        splits[static_cast<std::size_t>(piece)] =
            merge_split(a, a_size, b, b_size, count * piece / pieces, compare);
      }

      for(Difference piece = 0; piece < pieces; ++piece) {
        Difference out_begin = count * piece / pieces;
        Difference out_end = count * (piece + 1) / pieces;
        Difference a_begin = splits[static_cast<std::size_t>(piece)];
        Difference a_end = splits[static_cast<std::size_t>(piece) + 1];
        tasks.submit(workers_, [=, &compare] {
          std::merge(
              std::make_move_iterator(a + a_begin),
              std::make_move_iterator(a + a_end),
              std::make_move_iterator(b + (out_begin - a_begin)),
              std::make_move_iterator(b + (out_end - a_end)),
              destination + first + out_begin,
              compare);
        });
      }

      merged.push_back(first);
    }

    // With an odd number of runs, the last one has no partner and moves
    // across unchanged.
    if(run + 2 == runs.size()) {
      Difference first = runs[run];
      Difference count = runs[run + 1] - first;
      Difference pieces = pieces_for(count);
      for(Difference piece = 0; piece < pieces; ++piece) {
        Difference out_begin = first + count * piece / pieces;
        Difference out_end = first + count * (piece + 1) / pieces;
        tasks.submit(workers_, [=] {
          std::move(source + out_begin, source + out_end, destination + out_begin);
        });
      }

      merged.push_back(first);
    }

    merged.push_back(size);
    tasks.wait();
    runs = std::move(merged);
  }

  Executor& workers_;
  std::size_t min_chunk_size_;
};
} // namespace detail

// Sorts [begin, end) by compare, splitting the work into tasks for executor,
// which is anything with submit(std::function<void()>). The calling thread
// takes part. As with std::sort, equivalent elements can end up in any
// order, as the chunks are sorted with it.
template <typename Executor, typename Iterator, typename Sentinel, typename Compare>
void parallel_sort(Executor& executor, Iterator begin, Sentinel end, Compare&& compare) {
  detail::ParallelSorter<Executor> sorter(executor);
  sorter(begin, end, std::forward<Compare>(compare));
}

} // namespace daily

#endif // DAILY_ALGORITHM_PARALLELSORT_HPP
