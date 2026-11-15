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
#ifndef DAILY_FAST_IOSTREAM_DETAIL_FORMAT_HPP
#define DAILY_FAST_IOSTREAM_DETAIL_FORMAT_HPP
#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <streambuf>
#include <string>
#include <system_error>
#include <type_traits>

namespace daily { namespace detail {

// Long enough for any arithmetic type written by std::to_chars in its
// shortest round trip form.
inline constexpr std::size_t kMaxNumberChars = 64;

// Writes value to out in the shortest form that reads back as the same
// value, ignoring the locale.
template <typename OutIter, typename T>
OutIter write_number(OutIter out, T value) {
  char buf[kMaxNumberChars];
  std::to_chars_result r = std::to_chars(buf, buf + kMaxNumberChars, value);
  return std::copy(buf, r.ptr, out);
}

template <typename T>
concept HasFromChars = requires(char const* p, T& v) {
  std::from_chars(p, p, v);
};

template <typename T>
inline constexpr bool kCharType = std::is_same_v<T, char> ||
                                  std::is_same_v<T, signed char> ||
                                  std::is_same_v<T, unsigned char>;

// The characters std::isspace accepts in the C locale. Inlined because
// calling std::isspace for every character is measurably slower.
constexpr bool is_space(int c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

// One whitespace delimited token read from a stream, held without
// allocating. Characters beyond N are consumed but not stored, and set
// overflow.
template <std::size_t N>
class FixedInputString {
 public:
  FixedInputString() = default;

  // Skips leading whitespace, then reads up to the next whitespace or e.
  template <typename Iter>
  FixedInputString(Iter b, Iter e) {
    while(b != e && is_space(static_cast<unsigned char>(*b))) {
      ++b;
    }

    while(b != e && !is_space(static_cast<unsigned char>(*b))) {
      push_back(*b);
      ++b;
    }

    reached_end = b == e;
  }

  void push_back(char c) {
    if(length < N) {
      data[length++] = c;
    }
    else {
      overflow = true;
    }
  }

  std::array<char, N> data = {};
  std::size_t length = 0;
  bool overflow = false;
  bool reached_end = false;
};

// Reads one token from sb into s, skipping leading whitespace. Works on the
// stream buffer directly, which is much faster than istreambuf_iterator.
template <std::size_t N>
void read_token(std::streambuf& sb, FixedInputString<N>& s) {
  using Traits = std::char_traits<char>;
  Traits::int_type c = sb.sgetc();
  while(!Traits::eq_int_type(c, Traits::eof()) && is_space(c)) {
    c = sb.snextc();
  }
  while(!Traits::eq_int_type(c, Traits::eof()) && !is_space(c)) {
    s.push_back(Traits::to_char_type(c));
    c = sb.snextc();
  }
  s.reached_end = Traits::eq_int_type(c, Traits::eof());
}

// Parses the whole token into v. Returns false, and sets v to zero, if the
// token is empty, overflowed, or isn't entirely a number of type T.
// Parses the whole token into v. Returns false, and sets v to zero, if the
// token is empty, overflowed, out of range for T, or isn't entirely a number.
template <typename T, std::size_t N>
bool parse_number(FixedInputString<N> const& s, T& v) {
  char const* b = s.data.data();
  char const* e = b + s.length;
  // Accept a leading '+' as std::istream does. from_chars doesn't.
  if(b != e && *b == '+') {
    ++b;
  }

  bool parsed = false;
  if(b != e && !s.overflow) {
    if constexpr(HasFromChars<T>) {
      std::from_chars_result r = std::from_chars(b, e, v);
      parsed = r.ec == std::errc() && r.ptr == e;
    }
    else if constexpr(sizeof(T) == sizeof(double)) {
      // Some standard libraries lack from_chars for long double. Where it's
      // the same size as double, parsing as double loses nothing.
      double d = 0;
      std::from_chars_result r = std::from_chars(b, e, d);
      parsed = r.ec == std::errc() && r.ptr == e;
      v = d;
    }
    else {
      // strtold needs a terminated string and uses the C locale's decimal
      // point.
      std::array<char, N + 1> terminated = {};
      std::copy(b, e, terminated.begin());
      char* end = nullptr;
      errno = 0;
      v = std::strtold(terminated.data(), &end);
      parsed = errno == 0 && end == terminated.data() + (e - b);
    }
  }

  if(!parsed) {
    v = T();
    return false;
  }

  return true;
}
}} // namespace daily::detail

#endif // DAILY_FAST_IOSTREAM_DETAIL_FORMAT_HPP
