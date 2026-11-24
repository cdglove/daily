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
#ifndef DAILY_FAST_IOSTREAM_FASTOSTREAM_HPP
#define DAILY_FAST_IOSTREAM_FASTOSTREAM_HPP
#pragma once

#include <array>
#include <charconv>
#include <cstring>
#include <ios>
#include <string_view>
#include <type_traits>

#include "daily/fast_iostream/detail/Format.hpp"

namespace daily {

// Wraps an output stream and writes numbers, characters and strings straight
// to its stream buffer, skipping the locale and the stream's formatting
// flags such as width and precision. Numbers are written in the shortest
// form that reads back as the same value. Anything else is written through
// the stream.
template <typename Stream>
class FastOStream {
 public:
  explicit FastOStream(Stream& stream)
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
inline constexpr bool kFastFormatted = std::is_arithmetic_v<T> &&
                                       !std::is_same_v<T, bool> && !kCharType<T>;

// Does what std::ostream::sentry does, then writes to the stream buffer.
template <typename Stream, typename Write>
void write_raw(Stream& stream, Write write) {
  if(!stream.good()) {
    return;
  }
  if(stream.tie()) {
    stream.tie()->flush();
  }
  if(!write(*stream.rdbuf())) {
    stream.setstate(std::ios_base::badbit);
  }
  // As std::ostream::sentry's destructor does. Calling stream.flush() would
  // sync twice, because flush() has its own sentry.
  if((stream.flags() & std::ios_base::unitbuf) && stream.rdbuf()->pubsync() == -1) {
    stream.setstate(std::ios_base::badbit);
  }
}

template <typename Stream>
void write_raw(Stream& stream, char const* data, std::streamsize size) {
  write_raw(stream, [&](std::streambuf& sb) {
    return sb.sputn(data, size) == size;
  });
}

} // namespace detail

template <typename Stream, typename T>
  requires detail::kFastFormatted<T>
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, T v) {
  std::array<char, detail::kMaxNumberChars> buf;
  char const* end = std::to_chars(buf.data(), buf.data() + buf.size(), v).ptr;
  detail::write_raw(o.stream(), buf.data(), end - buf.data());
  return o;
}

template <typename Stream, typename T>
  requires detail::kCharType<T>
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, T v) {
  detail::write_raw(o.stream(), [v](std::streambuf& sb) {
    return !std::char_traits<char>::eq_int_type(
        sb.sputc(static_cast<char>(v)),
        std::char_traits<char>::eof());
  });
  return o;
}

template <typename Stream>
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, std::string_view v) {
  detail::write_raw(o.stream(), v.data(), static_cast<std::streamsize>(v.size()));
  return o;
}

template <typename Stream>
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, signed char const* v) {
  return o << std::string_view(reinterpret_cast<char const*>(v));
}

template <typename Stream>
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, unsigned char const* v) {
  return o << std::string_view(reinterpret_cast<char const*>(v));
}

// Everything else is written by the stream.
template <typename Stream, typename T>
  requires(!std::is_arithmetic_v<T> && !std::is_convertible_v<T const&, std::string_view>)
FastOStream<Stream>& operator<<(FastOStream<Stream>& o, T const& v) {
  o.stream() << v;
  return o;
}

} // namespace daily

#endif // DAILY_FAST_IOSTREAM_FASTOSTREAM_HPP
