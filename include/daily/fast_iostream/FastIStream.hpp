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
#ifndef DAILY_FAST_IOSTREAM_FASTISTREAM_HPP
#define DAILY_FAST_IOSTREAM_FASTISTREAM_HPP
#pragma once

#include <ios>
#include <streambuf>
#include <type_traits>

#include "daily/fast_iostream/detail/Format.hpp"

namespace daily {

// Wraps an input stream and reads numbers straight from its stream buffer,
// skipping the locale and the stream's formatting flags. Anything else is
// read by the stream. The counterpart of FastOStream.
template <typename Stream>
class FastIStream {
 public:
  explicit FastIStream(Stream& stream)
      : stream_(stream) {
  }

  Stream& stream() {
    return stream_;
  }
  Stream const& stream() const {
    return stream_;
  }

 private:
  Stream& stream_;
};

namespace detail {
template <typename T>
inline constexpr bool kFastParsed = std::is_arithmetic_v<T> &&
                                    !std::is_same_v<T, bool> && !kCharType<T>;
} // namespace detail

// Reads a number straight from the stream buffer. Sets failbit, and sets v
// to zero, if the next token isn't entirely a number of type T. Sets eofbit
// if the token ends at the end of the stream.
template <typename Stream, typename T>
  requires detail::kFastParsed<T>
FastIStream<Stream>& operator>>(FastIStream<Stream>& is, T& v) {
  Stream& stream = is.stream();
  // What std::istream::sentry does, apart from skipping whitespace, which
  // read_token does.
  if(!stream.good()) {
    stream.setstate(std::ios_base::failbit);
    v = T();
    return is;
  }
  if(stream.tie()) {
    stream.tie()->flush();
  }

  detail::FixedInputString<32> s;
  detail::read_token(*stream.rdbuf(), s);
  std::ios_base::iostate state = std::ios_base::goodbit;
  if(s.reached_end) {
    state |= std::ios_base::eofbit;
  }
  if(!detail::parse_number(s, v)) {
    state |= std::ios_base::failbit;
  }
  stream.setstate(state);
  return is;
}

// Everything else is read by the stream.
template <typename Stream, typename T>
  requires(!detail::kFastParsed<T>)
FastIStream<Stream>& operator>>(FastIStream<Stream>& is, T& v) {
  is.stream() >> v;
  return is;
}

} // namespace daily

#endif // DAILY_FAST_IOSTREAM_FASTISTREAM_HPP
