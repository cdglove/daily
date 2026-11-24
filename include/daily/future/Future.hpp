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
#ifndef DAILY_FUTURE_FUTURE_HPP
#define DAILY_FUTURE_FUTURE_HPP
#pragma once

#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "daily/future/DefaultAllocator.hpp"

// A future and promise in the style of std::future and std::promise, with
// continuations: future::then gives a new future for the result of a
// function of this one's result. Where the continuation runs is chosen with
// a tag:
//
//   continue_on::set runs it on the thread that sets the result.
//   continue_on::get runs it on the thread that asks for the continuation's
//     result, when it does.
//   continue_on::any runs it on whichever of those comes first. Plain
//     then(f) does this.
//   execute::dispatch, execute::post and execute::defer submit it to an
//     executor, with its function of the same name, as Asio's do.
//
// Each step of a chain shares the mutex of the promise at its root. promise,
// packaged_task and then can be given an allocator for the shared states,
// which is future_default_allocator if they aren't.

namespace daily {
template <typename Result>
class future;

template <typename Result>
class promise;

// Whether a wait for a future ended because it was ready, or because the
// time was up.
enum class future_status {
  ready,
  timeout,
};

// Why a future_error was thrown, as std::future_errc.
enum class future_errc {
  broken_promise,
  future_already_retrieved,
  promise_already_satisfied,
  no_state,
};

// The tags for where future::then runs a continuation that isn't given an
// executor. See the top of this file.
namespace continue_on {
struct any_t {
} constexpr any;
struct get_t {
} constexpr get;
struct set_t {
} constexpr set;
}; // namespace continue_on

// The tags for how future::then submits a continuation to an executor. See
// the top of this file.
namespace execute {
struct dispatch_t {
} constexpr dispatch;
struct post_t {
} constexpr post;
struct defer_t {
} constexpr defer;
}; // namespace execute

// Thrown for misuse, such as setting a promise's result twice, as
// std::future_error.
class future_error : public std::logic_error {
 public:
  explicit future_error(future_errc err)
      : logic_error("")
      , error_(err) {
  }

  future_errc code() const {
    return error_;
  }

 private:
  future_errc error_;
};

namespace detail {
// Unlocks a lock for its lifetime, and locks it again when destroyed, unless
// something else has.
class reverse_lock {
  std::unique_lock<std::mutex>& lock_;

 public:
  explicit reverse_lock(std::unique_lock<std::mutex>& lk)
      : lock_(lk) {
    lock_.unlock();
  }

  ~reverse_lock() {
    if(!lock_.owns_lock()) {
      lock_.lock();
    }
  }

  void lock() {
    if(!lock_.owns_lock()) {
      lock_.lock();
    }
  }

  reverse_lock& operator=(reverse_lock const&) = delete;
  reverse_lock(reverse_lock const&) = delete;
};

// Base shared state used by all derived shared states.
class future_shared_state_base {
 public:
  // No copying or moving, pointer semantic only.
  future_shared_state_base(future_shared_state_base const&) = delete;
  future_shared_state_base& operator=(future_shared_state_base const&) = delete;
  future_shared_state_base(future_shared_state_base&&) = delete;
  future_shared_state_base& operator=(future_shared_state_base&&) = delete;

  virtual ~future_shared_state_base() = default;

  future_shared_state_base() = default;

  void set_finished(std::unique_lock<std::mutex>& lock) {
    finished_ = true;
    ready_wait_.notify_all();
    if(continuation_) {
      continuation_->continuation_result_ready(lock);
    }
  }

  void set_finished_with_exception(
      std::exception_ptr p,
      std::unique_lock<std::mutex>& lock) {
    exception_ = std::move(p);
    // Don't call set finished because we don't want to run the continuations.
    // Pass the exception on to the continuation's future instead.
    finished_ = true;
    ready_wait_.notify_all();
    if(continuation_) {
      continuation_->set_finished_with_exception(exception_, lock);
    }
  }

  bool is_finished(std::unique_lock<std::mutex>&) const {
    return finished_;
  }

  bool has_exception(std::unique_lock<std::mutex>&) const {
    return static_cast<bool>(exception_);
  }

  void set_invalid(std::unique_lock<std::mutex>&) {
    is_valid_ = false;
  }

  bool is_valid(std::unique_lock<std::mutex>&) const {
    return is_valid_;
  }

  void set_continuation(
      std::shared_ptr<future_shared_state_base> continuation,
      std::unique_lock<std::mutex>& lock) {
    continuation_ = std::move(continuation);
    if(finished_) {
      if(exception_) {
        continuation_->set_finished_with_exception(exception_, lock);
      }
      else {
        continuation_->continuation_result_ready(lock);
      }
    }
  }

  void do_wait_result(std::unique_lock<std::mutex>& lock) {
    if(!finished_) {
      continuation_result_requested(lock);
    }

    do_wait(lock);
  }

  void check_exception(std::unique_lock<std::mutex>& /*lock*/) {
    if(exception_) {
      std::rethrow_exception(exception_);
    }
  }

  void do_wait(std::unique_lock<std::mutex>& lock) {
    while(!finished_) {
      ready_wait_.wait(lock);
    }
  }

  template <typename Rep, typename Period>
  future_status do_wait_for(
      std::chrono::duration<Rep, Period> const& rel_time,
      std::unique_lock<std::mutex>& lock) {
    ready_wait_.wait_for(lock, rel_time, [this] { return finished_; });
    return finished_ ? future_status::ready : future_status::timeout;
  }

  template <typename Clock, typename Duration>
  future_status do_wait_until(
      std::chrono::time_point<Clock, Duration> const& abs_time,
      std::unique_lock<std::mutex>& lock) {
    ready_wait_.wait_until(lock, abs_time, [this] { return finished_; });
    return finished_ ? future_status::ready : future_status::timeout;
  }

  // Only implemented by continuation derived shared_state types
  void continuation_result_ready(std::unique_lock<std::mutex>& lock) {
    handle_continuation_result_ready(lock);
  }

  void continuation_result_requested(std::unique_lock<std::mutex>& lock) {
    handle_continuation_result_requested(lock);
  }

 private:
  // Only implemented by continuation derived shared_state types
  virtual void handle_continuation_result_ready(std::unique_lock<std::mutex>&) {
  }

  virtual void handle_continuation_result_requested(std::unique_lock<std::mutex>& lock) {
    while(!finished_) {
      ready_wait_.wait(lock);
    }
  }

  std::exception_ptr exception_;
  std::condition_variable ready_wait_;
  std::shared_ptr<future_shared_state_base> continuation_;
  bool finished_{false};
  bool is_valid_{true};
};

// Base shared state used by all dreived shares states -- adds the result.
template <typename Result>
class future_shared_state : public future_shared_state_base {
 public:
  using storage_type = boost::optional<Result>;

  void set_finished_with_result(Result r, std::unique_lock<std::mutex>& lock) {
    result_ = std::move(r);
    set_finished(lock);
  }

  Result get(std::unique_lock<std::mutex>& lock) {
    set_invalid(lock);
    this->check_exception(lock);
    return *std::move(result_);
  }

  bool has_value(std::unique_lock<std::mutex>&) const {
    return result_;
  }

 private:
  storage_type result_;
};

// Base shared state used by all derived shares states -- adds the result
// with a void specialization.
template <>
class future_shared_state<void> : public future_shared_state_base {
 public:
  using storage_tyoe = void;

  void set_finished_with_result(std::unique_lock<std::mutex>& lock) {
    set_finished(lock);
  }

  void get(std::unique_lock<std::mutex>& lock) {
    set_invalid(lock);
    this->check_exception(lock);
  }
};

// Base shared state used by all dreived shares states -- adds the result
// with a ref specialization.
template <typename Result>
class future_shared_state<Result&> : public future_shared_state_base {
 public:
  using storage_type = Result*;

  future_shared_state()
      : result_(nullptr) {
  }

  void set_finished_with_result(Result& r, std::unique_lock<std::mutex>& lock) {
    result_ = &r;
    set_finished(lock);
  }

  Result& get(std::unique_lock<std::mutex>& lock) {
    set_invalid(lock);
    this->check_exception(lock);
    return *result_;
  }

 private:
  storage_type result_;
};

// Shared state initially created by the promise. Contains the mutex
// used to lock the system.
template <typename Result>
class promise_future_shared_state : public future_shared_state<Result> {
 public:
  std::unique_lock<std::mutex> lock() const {
    return std::unique_lock<std::mutex>(mutex_);
  }

 private:
  template <typename>
  friend class daily::promise;

  mutable std::mutex mutex_;
};

// Calls a continuation with its parent's result, and sets its own result to
// what it returns. Specialised for continuations that take or return
// nothing.
template <typename Param, typename Return>
struct continue_on_continuation_helper;

template <typename Param, typename Return>
struct continue_on_continuation_helper {
  template <typename Caller>
  static void call(Caller* caller, std::unique_lock<std::mutex>& lock) {
    // Don't call user code with the lock still obtained.
    lock.unlock();
    Return current_result = caller->continuation_(caller->parent_->get(lock));
    lock.lock();
    // Setting the result runs the continuations that follow this one and
    // run where results are set, and rethrows what they throw, see
    // do_continue.
    caller->set_finished_with_result(std::move(current_result), lock);
  }
};

// Basic Continuations.
template <typename Return>
struct continue_on_continuation_helper<void, Return> {
  template <typename Caller>
  static void call(Caller* caller, std::unique_lock<std::mutex>& lock) {
    // Don't call user code with the lock still obtained.
    lock.unlock();
    Return current_result = caller->continuation_();
    lock.lock();
    // Setting the result runs the continuations that follow this one and
    // run where results are set, and rethrows what they throw, see
    // do_continue.
    caller->set_finished_with_result(std::move(current_result), lock);
  }
};

template <typename Param>
struct continue_on_continuation_helper<Param, void> {
  template <typename Caller>
  static void call(Caller* caller, std::unique_lock<std::mutex>& lock) {
    // Don't call user code with the lock still obtained.
    lock.unlock();
    caller->continuation_(caller->parent_->get(lock));
    lock.lock();
    // Setting the result runs the continuations that follow this one and
    // run where results are set, and rethrows what they throw, see
    // do_continue.
    caller->set_finished_with_result(lock);
  }
};

template <>
struct continue_on_continuation_helper<void, void> {
  template <typename Caller>
  static void call(Caller* caller, std::unique_lock<std::mutex>& lock) {
    // Don't call user code with the lock still obtained.
    lock.unlock();
    caller->continuation_();
    lock.lock();
    // Setting the result runs the continuations that follow this one and
    // run where results are set, and rethrows what they throw, see
    // do_continue.
    caller->set_finished_with_result(lock);
  }
};

// The shared state of a continuation that isn't given an executor. The
// derived states decide when it runs, see continue_on.
template <typename ParentResult, typename Result, typename Function>
class continue_on_basic_shared_state : public future_shared_state<Result> {
 public:
  continue_on_basic_shared_state(future_shared_state<ParentResult>* parent, Function&& f)
      : parent_(std::move(parent))
      , continuation_(std::move(f)) {
  }

  ~continue_on_basic_shared_state() override = 0;

 protected:
  template <typename Param, typename Return>
  friend struct continue_on_continuation_helper;

  // Runs the continuation, and stores what it throws as its result.
  //
  // This doesn't work for an exception thrown after the continuation
  // returns. Setting the result runs any continuation after this one that
  // runs where results are set, which rethrows what it throws. That reaches
  // the catch below with the lock held, where locking it again throws
  // std::system_error, which escapes to whoever set the first result.
  void do_continue(std::unique_lock<std::mutex>& lock) {
    try {
      continue_on_continuation_helper<ParentResult, Result>::call(this, lock);
    }
    catch(...) {
      lock.lock();
      this->set_finished_with_exception(std::current_exception(), lock);
    }
  } // namespace detail

  // Don't store a shared_ptr here because as long as we're alive the parent
  // must be too.
  future_shared_state<ParentResult>* parent_;
  Function continuation_;
}; // namespace daily

template <typename Future, typename Result, typename Function>
inline continue_on_basic_shared_state<Future, Result, Function>::~continue_on_basic_shared_state() =
    default;

// Runs when the parent's result is set or this one's is asked for,
// whichever is first.
template <typename ParentResult, typename Result, typename Function>
class continue_on_any_shared_state
    : public continue_on_basic_shared_state<ParentResult, Result, Function> {
 public:
  using continue_on_basic_shared_state<ParentResult, Result, Function>::continue_on_basic_shared_state;

 private:
  void handle_continuation_result_ready(std::unique_lock<std::mutex>& lock) override {
    this->do_continue(lock);
    this->check_exception(lock);
  }

  void handle_continuation_result_requested(std::unique_lock<std::mutex>& lock) override {
    this->do_continue(lock);
  }
};

// Runs when the parent's result is set.
template <typename ParentResult, typename Result, typename Function>
class continue_on_set_shared_state
    : public continue_on_basic_shared_state<ParentResult, Result, Function> {
 public:
  using continue_on_basic_shared_state<ParentResult, Result, Function>::continue_on_basic_shared_state;

 private:
  void handle_continuation_result_ready(std::unique_lock<std::mutex>& lock) override {
    this->do_continue(lock);
    this->check_exception(lock);
  }

  void handle_continuation_result_requested(std::unique_lock<std::mutex>& lock) override {
    this->parent_->continuation_result_requested(lock);
  }
};

// Runs when this one's result is asked for.
template <typename ParentResult, typename Result, typename Function>
class continue_on_get_shared_state
    : public continue_on_basic_shared_state<ParentResult, Result, Function> {
 public:
  using continue_on_basic_shared_state<ParentResult, Result, Function>::continue_on_basic_shared_state;

 private:
  void handle_continuation_result_requested(std::unique_lock<std::mutex>& lock) override {
    this->parent_->continuation_result_requested(lock);
    // The parent's exception may have been passed on already.
    if(!this->is_finished(lock)) {
      this->do_continue(lock);
    }
  }
};

// Make the shared state of a continuation for each continue_on tag.
template <typename Result, typename ParentResult, typename Function, typename Allocator>
auto make_continue_on_continuation(
    continue_on::any_t,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    Allocator const& alloc) {
  return std::allocate_shared<continue_on_any_shared_state<ParentResult, Result, Function>>(
      alloc,
      parent,
      std::forward<Function>(func));
}

template <typename Result, typename ParentResult, typename Function, typename Allocator>
auto make_continue_on_continuation(
    continue_on::get_t,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    Allocator const& alloc) {
  return std::allocate_shared<continue_on_get_shared_state<ParentResult, Result, Function>>(
      alloc,
      parent,
      std::forward<Function>(func));
}

template <typename Result, typename ParentResult, typename Function, typename Allocator>
auto make_continue_on_continuation(
    continue_on::set_t,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    Allocator const& alloc) {
  return std::allocate_shared<continue_on_set_shared_state<ParentResult, Result, Function>>(
      alloc,
      parent,
      std::forward<Function>(func));
}

// Executor Continuations.

// Submit a closure to an executor with its dispatch, post or defer.
struct submit_dispatch {
  template <typename Executor, typename Closure, typename Allocator>
  static void submit(Executor&& ex, Closure&& c, Allocator const& alloc) {
    ex.dispatch(std::forward<Closure>(c), alloc);
  }
};

struct submit_post {
  template <typename Executor, typename Closure, typename Allocator>
  static void submit(Executor&& ex, Closure&& c, Allocator const& alloc) {
    ex.post(std::forward<Closure>(c), alloc);
  }
};

struct submit_defer {
  template <typename Executor, typename Closure, typename Allocator>
  static void submit(Executor&& ex, Closure&& c, Allocator const& alloc) {
    ex.defer(std::forward<Closure>(c), alloc);
  }
};

// Submits a closure that calls a continuation with its parent's result and
// sets its own result to what it returns. Specialised for continuations
// that take or return nothing.
template <typename Submiter, typename Param, typename Return>
struct executor_continuation_helper;

template <typename Submiter, typename Param, typename Return>
struct executor_continuation_helper {
  template <typename Caller, typename Allocator>
  static void call(
      Caller* caller,
      std::unique_lock<std::mutex>& lock,
      std::shared_ptr<std::mutex>& promise_mutex,
      Allocator const& alloc) {
    auto closure = [caller, p = caller->parent_->get(lock), promise_mutex] {
      auto result = caller->continuation_(std::move(p));
      std::unique_lock<std::mutex> lock(*promise_mutex);
      caller->set_finished_with_result(std::move(result), lock);
    };

    // Don't call user code with the lock still obtained.
    lock.unlock();
    Submiter::submit(caller->executor_, std::move(closure), alloc);
    lock.lock();
  }
};

template <typename Submiter, typename Return>
struct executor_continuation_helper<Submiter, void, Return> {
  template <typename Caller, typename Allocator>
  static void call(
      Caller* caller,
      std::unique_lock<std::mutex>& lock,
      std::shared_ptr<std::mutex>& promise_mutex,
      Allocator const& alloc) {
    auto closure = [caller, promise_mutex] {
      auto result = caller->continuation_();
      std::unique_lock<std::mutex> lock(*promise_mutex);
      caller->set_finished_with_result(std::move(result), lock);
    };

    // Don't call user code with the lock still obtained.
    lock.unlock();
    Submiter::submit(caller->executor_, std::move(closure), alloc);
    lock.lock();
  }
};

template <typename Submiter, typename Param>
struct executor_continuation_helper<Submiter, Param, void> {
  template <typename Caller, typename Allocator>
  static void call(
      Caller* caller,
      std::unique_lock<std::mutex>& lock,
      std::shared_ptr<std::mutex>& promise_mutex,
      Allocator const& alloc) {
    auto closure = [caller, p = caller->parent_->get(lock), promise_mutex] {
      caller->continuation_(std::move(p));
      std::unique_lock<std::mutex> lock(*promise_mutex);
      caller->set_finished_with_result(lock);
    };

    // Don't call user code with the lock still obtained.
    lock.unlock();
    Submiter::submit(caller->executor_, std::move(closure), alloc);
    lock.lock();
  }
};

template <typename Submiter>
struct executor_continuation_helper<Submiter, void, void> {
  template <typename Caller, typename Allocator>
  static void call(
      Caller* caller,
      std::unique_lock<std::mutex>& lock,
      std::shared_ptr<std::mutex>& promise_mutex,
      Allocator const& alloc) {
    auto closure = [caller, promise_mutex] {
      caller->continuation_();
      std::unique_lock<std::mutex> lock(*promise_mutex);
      caller->set_finished_with_result(lock);
    };

    // Don't call user code with the lock still obtained.
    lock.unlock();
    Submiter::submit(caller->executor_, closure, alloc);
    lock.lock();
  }
};

// The shared state of a continuation given an executor, which is submitted
// to it when the parent's result is set.
template <typename Submitter, typename Executor, typename ParentResult, typename Result, typename Function, typename Allocator>
class executor_continuation_shared_state : public future_shared_state<Result> {
 public:
  executor_continuation_shared_state(
      Executor ex,
      future_shared_state<ParentResult>* parent,
      Function&& f,
      std::shared_ptr<std::mutex> const& mutex,
      Allocator alloc)
      : parent_(std::move(parent))
      , executor_(std::move(ex))
      , continuation_(std::move(f))
      , promise_mutex_(mutex)
      , allocator_(alloc) {
  }

 private:
  template <typename, typename, typename>
  friend struct executor_continuation_helper;

  void handle_continuation_result_ready(std::unique_lock<std::mutex>& lock) override {
    // The promise mutex shares ownership of the root state, which owns this
    // state through the chain of continuations. Whoever is finishing the
    // parent holds the root, so this can't fail. The submitted closure keeps
    // its own reference, so the chain stays alive until it has run.
    std::shared_ptr<std::mutex> promise_mutex = promise_mutex_.lock();
    assert(promise_mutex && lock.mutex() == promise_mutex.get());
    executor_continuation_helper<Submitter, ParentResult, Result>::call(
        this,
        lock,
        promise_mutex,
        allocator_);
  }

  // Don't store a shared_ptr here because as long as we're alive the parent
  // must be too.
  future_shared_state<ParentResult>* parent_;
  Executor executor_;
  Function continuation_;
  // Weak, because the root state owns this one; a strong reference would be
  // a cycle that keeps the whole chain alive forever.
  std::weak_ptr<std::mutex> promise_mutex_;
  Allocator allocator_;
};

// Make the shared state of a continuation for each execute tag.
template <typename Result, typename Executor, typename ParentResult, typename Function, typename Allocator>
auto make_executor_continuation(
    execute::dispatch_t,
    Executor ex,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    std::shared_ptr<std::mutex> const& mutex,
    Allocator const& alloc) {
  using shared_state =
      executor_continuation_shared_state<submit_dispatch, Executor, ParentResult, Result, Function, Allocator>;

  return std::allocate_shared<shared_state>(
      alloc,
      std::move(ex),
      parent,
      std::forward<Function>(func),
      mutex,
      alloc);
}

template <typename Result, typename Executor, typename ParentResult, typename Function, typename Allocator>
auto make_executor_continuation(
    execute::post_t,
    Executor ex,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    std::shared_ptr<std::mutex> const& mutex,
    Allocator const& alloc) {
  using shared_state =
      executor_continuation_shared_state<submit_post, Executor, ParentResult, Result, Function, Allocator>;

  return std::allocate_shared<shared_state>(
      alloc,
      std::move(ex),
      parent,
      std::forward<Function>(func),
      mutex,
      alloc);
}

template <typename Result, typename Executor, typename ParentResult, typename Function, typename Allocator>
auto make_executor_continuation(
    execute::defer_t,
    Executor ex,
    future_shared_state<ParentResult>* parent,
    Function&& func,
    std::shared_ptr<std::mutex> const& mutex,
    Allocator const& alloc) {
  using shared_state =
      executor_continuation_shared_state<submit_defer, Executor, ParentResult, Result, Function, Allocator>;

  return std::allocate_shared<shared_state>(
      alloc,
      std::move(ex),
      parent,
      std::forward<Function>(func),
      mutex,
      alloc);
}
} // namespace detail

// Sets the result of the future it gives, as std::promise. Destroying it
// before setting a result breaks the promise: the future throws a
// future_error with future_errc::broken_promise.
template <typename Result = void>
class promise {
 private:
  using shared_state = detail::promise_future_shared_state<Result>;

 public:
  promise()
      : state_(std::make_shared<shared_state>()) {
  }

  template <typename Allocator>
  promise(std::allocator_arg_t, Allocator const& alloc)
      : state_(std::allocate_shared<shared_state>(alloc)) {
  }

  ~promise() {
    abandon();
  }

  promise(promise&& other) noexcept
      : state_(std::move(other.state_))
      , future_obtained_(std::exchange(other.future_obtained_, false)) {
  }

  promise& operator=(promise&& other) noexcept {
    if(this != &other) {
      abandon();
      state_ = std::move(other.state_);
      future_obtained_ = std::exchange(other.future_obtained_, false);
    }
    return *this;
  }

  promise(promise const& other) = delete;
  promise& operator=(promise const& rhs) = delete;

  void swap(promise& other) noexcept {
    std::swap(state_, other.state_);
    std::swap(future_obtained_, other.future_obtained_);
  }

  future<Result> get_future() {
    if(!state_) {
      BOOST_THROW_EXCEPTION(future_error(future_errc::no_state));
    }

    if(future_obtained_) {
      BOOST_THROW_EXCEPTION(future_error(future_errc::future_already_retrieved));
    }

    future_obtained_ = true;
    return future<Result>(state_, {state_, &state_->mutex_});
  }

  // Use a vararg here to avoid having to specialize the whole class for
  // lvalue ref and void.
  template <typename... R>
  void set_value(R&&... value) {
    static_assert(
        sizeof...(value) < 2,
        "set_value must be called with exactly 0 or 1 argument");

    auto lk = state_->lock();
    if(state_->is_finished(lk)) {
      BOOST_THROW_EXCEPTION(future_error(future_errc::promise_already_satisfied));
    }

    state_->set_finished_with_result(std::forward<R>(value)..., lk);
  }

  void set_exception(std::exception_ptr p) {
    auto lk = state_->lock();
    if(state_->is_finished(lk)) {
      BOOST_THROW_EXCEPTION(future_error(future_errc::promise_already_satisfied));
    }

    state_->set_finished_with_exception(std::move(p), lk);
  }

 private:
  // Breaks the promise if a future is waiting for a result that was never
  // set.
  void abandon() noexcept {
    if(state_) {
      auto lk = state_->lock();
      if(future_obtained_ && !state_->is_finished(lk)) {
        try {
          BOOST_THROW_EXCEPTION(future_error(future_errc::broken_promise));
        }
        catch(...) {
          try {
            state_->set_finished_with_exception(std::current_exception(), lk);
          }
          // Don't let exceptions escape from the dtor.
          // NOLINTNEXTLINE(bugprone-empty-catch)
          catch(...) {
          }
        }
      }
    }
  }

  std::shared_ptr<shared_state> state_;
  bool future_obtained_ = false;
};

template <typename Result>
void swap(promise<Result>& lhs, promise<Result>& rhs) noexcept {
  lhs.swap(rhs);
}

// The result of a promise or a packaged_task, as std::future, which then can
// add continuations to. Adding one moves this future's state into the
// continuation, so this one is no longer valid.
template <typename Result = void>
class future {
 private:
  using shared_state = detail::future_shared_state<Result>;
  using result_type = Result;

 public:
  future() noexcept
      : mutex_(nullptr) {
  }

  // move support
  future(future&& other) noexcept
      : state_(std::move(other.state_))
      , mutex_(std::move(other.mutex_)) {
  }

  future& operator=(future&& other) noexcept {
    if(&other != this) {
      state_ = std::move(other.state_);
      mutex_ = std::move(other.mutex_);
    }
    return *this;
  }

  // no copy
  future(future const& other) = delete;
  future& operator=(future const& other) = delete;

  Result get() {
    assert(valid());
    auto lk = lock();
    state_->do_wait_result(lk);
    return state_->get(lk);
  }

  bool valid() const noexcept {
    if(state_) {
      auto lk = lock();
      return state_->is_valid(lk);
    }

    return false;
  }

  void wait() const {
    auto lk = lock();
    assert(state_->is_valid(lk));
    state_->do_wait(lk);
  }

  bool is_ready() const {
    auto lk = lock();
    return state_->is_finished(lk);
  }

  bool has_exception() const {
    auto lk = lock();
    return state_->has_exception(lk);
  }

  bool has_value() const {
    auto lk = lock();
    return state_->has_value(lk);
  }

  template <typename Rep, typename Period>
  future_status wait_for(std::chrono::duration<Rep, Period> const& rel_time) const {
    auto lk = lock();
    assert(state_->is_valid(lk));
    return state_->do_wait_for(rel_time, lk);
  }

  template <typename Clock, typename Duration>
  future_status wait_until(std::chrono::time_point<Clock, Duration> const& abs_time) const {
    auto lk = lock();
    assert(state_->is_valid(lk));
    return state_->do_wait_until(abs_time, lk);
  }

  template <typename F, typename Allocator = future_default_allocator>
  auto then(F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return this->then(continue_on::any, std::forward<F>(f), alloc);
  }

  template <typename F, typename Allocator = future_default_allocator>
  auto then(continue_on::any_t a, F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return continue_on_then(a, std::forward<F>(f), alloc);
  }

  template <typename F, typename Allocator = future_default_allocator>
  auto then(continue_on::get_t g, F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return continue_on_then(g, std::forward<F>(f), alloc);
  }

  template <typename F, typename Allocator = future_default_allocator>
  auto then(continue_on::set_t s, F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return continue_on_then(s, std::forward<F>(f), alloc);
  }

  template <typename Executor, typename F, typename Allocator = future_default_allocator>
  auto then(
      execute::dispatch_t d,
      Executor& ex,
      F&& f,
      Allocator const& alloc = Allocator()) {
    assert(valid());
    return executor_then(d, ex, std::forward<F>(f), alloc);
  }

  template <typename Executor, typename F, typename Allocator = future_default_allocator>
  auto then(execute::post_t p, Executor& ex, F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return executor_then(p, ex, std::forward<F>(f), alloc);
  }

  template <typename Executor, typename F, typename Allocator = future_default_allocator>
  auto then(execute::defer_t d, Executor& ex, F&& f, Allocator const& alloc = Allocator()) {
    assert(valid());
    return executor_then(d, ex, std::forward<F>(f), alloc);
  }

 private:
  template <typename Function, typename Param>
  struct result_of;

  template <typename Function, typename Param>
  struct result_of {
    using type = decltype(std::declval<Function>()(std::declval<Param>()));
  };

  template <typename Function>
  struct result_of<Function, void> {
    using type = decltype(std::declval<Function>()());
  };

  template <typename Selector, typename F, typename Allocator>
  auto continue_on_then(Selector s, F&& f, Allocator const& alloc) {
    using ContinuationResult = result_of<F, Result>::type;

    // 'this' future is now invalidated so we move the state out
    // to indicate that.
    auto const current_state = std::move(state_);
    auto const continuation_state =
        detail::make_continue_on_continuation<ContinuationResult>(
            s,
            current_state.get(),
            std::forward<F>(f),
            alloc);

    auto lk = lock();
    current_state->set_continuation(continuation_state, lk);
    return future<ContinuationResult>(continuation_state, mutex_);
  }

  template <typename Selector, typename Executor, typename F, typename Allocator>
  auto executor_then(Selector s, Executor& ex, F&& f, Allocator const& alloc) {
    using ContinuationResult = result_of<F, Result>::type;

    // 'this' future is now invalidated so we move the state out
    // to indicate that.
    auto const current_state = std::move(state_);
    auto const continuation_state =
        detail::make_executor_continuation<ContinuationResult>(
            s,
            ex.get_executor(),
            current_state.get(),
            std::forward<F>(f),
            mutex_,
            alloc);

    auto lk = lock();
    current_state->set_continuation(continuation_state, lk);
    return future<ContinuationResult>(continuation_state, mutex_);
  }

  std::unique_lock<std::mutex> lock() const {
    return std::unique_lock<std::mutex>(*mutex_);
  }

  template <typename>
  friend class promise;

  template <typename>
  friend class future;

  template <typename, typename, typename>
  friend class detail::continue_on_basic_shared_state;

  explicit future(std::shared_ptr<shared_state> ss, std::shared_ptr<std::mutex> promise_mutex)
      : state_(std::move(ss))
      , mutex_(std::move(promise_mutex)) {
  }

  std::shared_ptr<shared_state> state_;
  std::shared_ptr<std::mutex> mutex_;
};

template <typename>
class packaged_task; // undefined

// Calls a function and sets the result of the future it gives to what it
// returns, or what it throws, as std::packaged_task.
template <class Result, typename... Args>
class packaged_task<Result(Args...)> {
 public:
  // Not noexcept, unlike std::packaged_task's, because the promise allocates
  // its shared state.
  packaged_task() = default;

  template <class F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, packaged_task>)
  explicit packaged_task(F&& f)
      : func_(std::forward<F>(f)) {
  }

  template <class F, class Allocator>
  packaged_task(std::allocator_arg_t, Allocator const& a, F&& f)
      : promise_(std::allocator_arg, a)
      , func_(std::forward<F>(f)) {
  }

  // no copy
  packaged_task(packaged_task const&) = delete;
  packaged_task& operator=(packaged_task const&) = delete;

  // move support
  packaged_task(packaged_task&& other) noexcept
      : promise_(std::move(other.promise_))
      , func_(std::move(other.func_)) {
  }

  packaged_task& operator=(packaged_task&& rhs) noexcept {
    promise_ = std::move(rhs.promise_);
    func_ = std::move(rhs.func_);
    return *this;
  }

  void swap(packaged_task& other) noexcept {
    promise_.swap(other.promise_);
    func_.swap(other.func_);
  }

  bool valid() const noexcept {
    return static_cast<bool>(func_);
  }

  // result retrieval
  future<Result> get_future() {
    return promise_.get_future();
  }

  // execution
  void operator()(Args... args) {
    try {
      if constexpr(std::is_void_v<Result>) {
        func_(std::forward<Args>(args)...);
        promise_.set_value();
      }
      else {
        promise_.set_value(func_(std::forward<Args>(args)...));
      }
    }
    catch(...) {
      promise_.set_exception(std::current_exception());
    }
  }

  // Abandons the current state and creates a new one, so the task can be
  // called again.
  void reset() {
    promise_ = promise<Result>();
  }

 private:
  promise<Result> promise_;
  std::function<Result(Args...)> func_;
};

template <typename Result, typename... Args>
void swap(packaged_task<Result(Args...)>& lhs, packaged_task<Result(Args...)>& rhs) noexcept {
  lhs.swap(rhs);
}
}; // namespace daily

// promise and packaged_task take allocators, as their std counterparts do.
namespace std {
template <typename Result, typename Alloc>
struct uses_allocator<daily::promise<Result>, Alloc> : std::true_type {};

template <typename Signature, typename Alloc>
struct uses_allocator<daily::packaged_task<Signature>, Alloc> : std::true_type {};
} // namespace std

#endif // DAILY_FUTURE_FUTURE_HPP
