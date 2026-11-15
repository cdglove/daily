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
#include "daily/trace/Log.hpp"

#include <iostream>
#include <streambuf>

namespace daily {
namespace detail {

std::atomic<unsigned int> g_log_verbosity = 0;
std::atomic<std::ostream*> g_log_sink = &std::clog;

namespace {
class NullBuffer : public std::streambuf {
 protected:
  int_type overflow(int_type c) override {
    return traits_type::not_eof(c);
  }
};
} // namespace

std::ostream& null_log_sink() {
  static NullBuffer buffer;
  static std::ostream stream(&buffer);
  return stream;
}

} // namespace detail

void set_log_verbosity(unsigned int verbosity) {
  detail::g_log_verbosity.store(verbosity, std::memory_order_relaxed);
}

void set_log_sink(std::ostream& out) {
  detail::g_log_sink.store(&out, std::memory_order_relaxed);
}

} // namespace daily
