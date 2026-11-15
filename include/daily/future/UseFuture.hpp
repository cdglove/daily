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
#ifndef DAILY_FUTURE_USEFUTURE_HPP
#define DAILY_FUTURE_USEFUTURE_HPP
#pragma once

#include <boost/asio/async_result.hpp>
#include <memory>
#include <utility>

#include "daily/future/DefaultAllocator.hpp"
#include "daily/future/Future.hpp"

namespace daily {

// A Boost.Asio completion token that makes an asynchronous operation return
// a daily::future, so continuations can be attached with then(). Supports
// completion signatures with zero or one argument.
template <typename Allocator = future_default_allocator>
struct use_future_t {
  constexpr use_future_t() noexcept = default;

  constexpr explicit use_future_t(Allocator alloc) noexcept
      : allocator_(std::move(alloc)) {
  }

  Allocator get_allocator() const noexcept {
    return allocator_;
  }

 private:
  Allocator allocator_;
};

inline constexpr use_future_t<> use_future;

// The completion handler created for use_future. Sets the value of the
// promise behind the returned future.
template <typename... Args>
class promise_handler {
 public:
  using promise_type = promise<Args...>;

  template <typename Allocator>
  explicit promise_handler(use_future_t<Allocator> const& token)
      : promise_(std::allocator_arg, token.get_allocator()) {
  }

  void operator()(Args... args) {
    promise_.set_value(std::move(args)...);
  }

  promise_type promise_;
};

} // namespace daily

namespace boost::asio {
template <typename Allocator, typename R, typename... Args>
class async_result<daily::use_future_t<Allocator>, R(Args...)> {
 public:
  using return_type = daily::future<Args...>;

  template <typename Initiation, typename... InitArgs>
  static return_type initiate(
      Initiation&& initiation,
      daily::use_future_t<Allocator> const& token,
      InitArgs&&... args) {
    daily::promise_handler<Args...> handler(token);
    return_type f = handler.promise_.get_future();
    std::forward<Initiation>(
        initiation)(std::move(handler), std::forward<InitArgs>(args)...);
    return f;
  }
};
} // namespace boost::asio

#endif // DAILY_FUTURE_USEFUTURE_HPP
