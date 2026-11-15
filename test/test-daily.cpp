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
#define BOOST_TEST_MODULE daily
#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <iostream>

#if defined(BOOST_ENABLE_ASSERT_HANDLER)

namespace boost {
void assertion_failed(char const* expr, char const* function, char const* file, long line) {
  std::cerr << "Assertion failed: " << expr << " in " << function << " ("
            << file << ":" << line << ")\n";
  std::abort();
}

void assertion_failed_msg(
    char const* expr,
    char const* msg,
    char const* function,
    char const* file,
    long line) {
  std::cerr << "Assertion failed: " << msg << " (" << expr << ") in "
            << function << " (" << file << ":" << line << ")\n";
  std::abort();
}
} // namespace boost
#endif
