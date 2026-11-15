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
#ifndef DAILY_TRACE_LOG_HPP
#define DAILY_TRACE_LOG_HPP
#pragma once

#include <atomic>
#include <ostream>
#include <utility>

// Verbosity based logging. log(n) returns a Logger that writes to the sink
// if the verbosity set with set_log_verbosity() is at least n, and discards
// everything otherwise.
//
// Building with DAILY_ENABLE_LOGGING=0 discards everything regardless of
// verbosity. Building with DAILY_ENABLE_FAST_LOG=1 formats numbers with
// FastOStream.
#ifndef DAILY_ENABLE_LOGGING
#  define DAILY_ENABLE_LOGGING 1
#endif

#ifndef DAILY_ENABLE_FAST_LOG
#  define DAILY_ENABLE_FAST_LOG 0
#endif

#if DAILY_ENABLE_FAST_LOG
#  include "daily/fast_iostream/FastOStream.hpp"
#endif

namespace daily {

class Logger {
 public:
#if DAILY_ENABLE_FAST_LOG
  using Sink = FastOStream<std::ostream>;
#else
  using Sink = std::ostream;
#endif

  explicit Logger(std::ostream& out)
      : sink_(out) {
  }

  template <typename T>
  Logger& operator<<(T&& t) {
#if DAILY_ENABLE_LOGGING
    sink_ << std::forward<T>(t);
#endif
    return *this;
  }

  // Allows io manipulators such as std::endl.
  Logger& operator<<(std::ostream& (*f)(std::ostream&)) {
#if DAILY_ENABLE_LOGGING
#  if DAILY_ENABLE_FAST_LOG
    sink_.stream() << f;
#  else
    sink_ << f;
#  endif
#endif
    return *this;
  }

  Sink& sink() {
    return sink_;
  }

 private:
#if DAILY_ENABLE_FAST_LOG
  // FastOStream only holds a reference to the stream.
  Sink sink_;
#else
  Sink& sink_;
#endif
};

// Messages logged with a verbosity above this are discarded. Defaults to 0.
void set_log_verbosity(unsigned int verbosity);

// Sets where messages are written. Defaults to std::clog. out must outlive
// all logging.
void set_log_sink(std::ostream& out);

namespace detail {
extern std::atomic<unsigned int> g_log_verbosity;
extern std::atomic<std::ostream*> g_log_sink;
std::ostream& null_log_sink();
} // namespace detail

inline Logger log(unsigned int verbosity = 0) {
  if(detail::g_log_verbosity.load(std::memory_order_relaxed) >= verbosity) {
    return Logger(*detail::g_log_sink.load(std::memory_order_relaxed));
  }
  return Logger(detail::null_log_sink());
}

} // namespace daily

#endif // DAILY_TRACE_LOG_HPP
