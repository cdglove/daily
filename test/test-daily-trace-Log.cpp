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
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "daily/trace/Log.hpp"

// Built twice: in test-daily with the defaults, and in
// test-daily-trace-fast-log with DAILY_ENABLE_FAST_LOG=1. The expected output
// is the same either way.
#if DAILY_ENABLE_FAST_LOG
static_assert(std::is_same_v<daily::Logger::Sink, daily::FastOStream<std::ostream>>);
#else
static_assert(std::is_same_v<daily::Logger::Sink, std::ostream>);
#endif

namespace {

// Captures log output and restores the defaults afterwards.
struct CaptureLog {
  CaptureLog() {
    daily::set_log_sink(out);
  }

  ~CaptureLog() {
    daily::set_log_sink(std::clog);
    daily::set_log_verbosity(0);
  }

  std::ostringstream out;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(daily_trace_log, CaptureLog)

BOOST_AUTO_TEST_CASE(writes_at_default_verbosity) {
  daily::log() << "value " << 42 << '\n';
  BOOST_TEST(out.str() == "value 42\n");
}

BOOST_AUTO_TEST_CASE(discards_above_verbosity) {
  daily::log(1) << "hidden";
  BOOST_TEST(out.str().empty());
}

BOOST_AUTO_TEST_CASE(verbosity_filters) {
  daily::set_log_verbosity(2);
  daily::log(1) << "a";
  daily::log(2) << "b";
  daily::log(3) << "c";
  BOOST_TEST(out.str() == "ab");
}

BOOST_AUTO_TEST_CASE(manipulators) {
  daily::log() << "line" << '\n';
  BOOST_TEST(out.str() == "line\n");
}

BOOST_AUTO_TEST_CASE(discarded_manipulators) {
  daily::log(1) << "line" << '\n';
  BOOST_TEST(out.str().empty());
}

BOOST_AUTO_TEST_CASE(numbers) {
  daily::log() << -12 << ' ' << 34u << ' ' << 123456789012LL << ' ' << 2.5;
  BOOST_TEST(out.str() == "-12 34 123456789012 2.5");
}

BOOST_AUTO_TEST_CASE(strings) {
  std::string s = "string";
  daily::log() << s << ' ' << "literal";
  BOOST_TEST(out.str() == "string literal");
}

BOOST_AUTO_TEST_SUITE_END()
