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
#include <limits>
#include <random>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>

#include "daily/fast_iostream/FastIStream.hpp"
#include "daily/fast_iostream/FastLocale.hpp"
#include "daily/fast_iostream/FastOStream.hpp"

namespace {

template <typename T>
std::string fast_format(T const& value) {
  std::ostringstream out;
  daily::FastOStream<std::ostream> fast(out);
  fast << value;
  return out.str();
}

} // namespace

BOOST_AUTO_TEST_SUITE(daily_fast_ostream)

BOOST_AUTO_TEST_CASE(integers) {
  BOOST_TEST(fast_format(0) == "0");
  BOOST_TEST(fast_format(42) == "42");
  BOOST_TEST(fast_format(-42) == "-42");
  BOOST_TEST(fast_format(short(-7)) == "-7");
  BOOST_TEST(fast_format(123456789012LL) == "123456789012");
  BOOST_TEST(fast_format(42u) == "42");
  BOOST_TEST(
      fast_format(std::numeric_limits<unsigned long long>::max()) ==
      std::to_string(std::numeric_limits<unsigned long long>::max()));
  BOOST_TEST(
      fast_format(std::numeric_limits<int>::min()) ==
      std::to_string(std::numeric_limits<int>::min()));
}

BOOST_AUTO_TEST_CASE(reals) {
  BOOST_TEST(fast_format(1.5) == "1.5");
  BOOST_TEST(fast_format(-0.25) == "-0.25");
  BOOST_TEST(fast_format(2.5f) == "2.5");
}

BOOST_AUTO_TEST_CASE(reals_round_trip) {
  BOOST_TEST(fast_format(0.1) == "0.1");
  BOOST_TEST(fast_format(3.14159) == "3.14159");
  BOOST_TEST(fast_format(1.0 / 3) == "0.3333333333333333");
  std::mt19937_64 rng(1);
  std::uniform_real_distribution<double> real(-1e9, 1e9);
  for(int i = 0; i < 1000; ++i) {
    double value = real(rng);
    std::istringstream in(fast_format(value));
    daily::FastIStream<std::istream> fast(in);
    double back = 0;
    fast >> back;
    BOOST_TEST(back == value);
  }
}

BOOST_AUTO_TEST_CASE(characters_and_strings) {
  BOOST_TEST(fast_format('x') == "x");
  BOOST_TEST(fast_format("text") == "text");
  BOOST_TEST(fast_format(std::string("string")) == "string");
}

BOOST_AUTO_TEST_CASE(chained) {
  std::ostringstream out;
  daily::FastOStream<std::ostream> fast(out);
  fast << 1 << ' ' << 2.5 << ' ' << "three";
  BOOST_TEST(out.str() == "1 2.5 three");
}

BOOST_AUTO_TEST_CASE(string_types) {
  BOOST_TEST(fast_format(std::string_view("view")) == "view");
  // An array of signed char is what this formats.
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  signed char const s[] = "signed";
  BOOST_TEST(fast_format(s) == "signed");
  BOOST_TEST(fast_format(static_cast<unsigned char>('u')) == "u");
}

BOOST_AUTO_TEST_CASE(ignores_width) {
  std::ostringstream out;
  out.width(10);
  daily::FastOStream<std::ostream> fast(out);
  fast << 42;
  BOOST_TEST(out.str() == "42");
}

BOOST_AUTO_TEST_CASE(failed_stream_writes_nothing) {
  std::ostringstream out;
  out.setstate(std::ios_base::failbit);
  daily::FastOStream<std::ostream> fast(out);
  fast << 42 << 'c' << "text";
  BOOST_TEST(out.str().empty());
}

// A stream buffer that discards output and counts flushes.
struct SyncCounter : std::streambuf {
  int_type overflow(int_type c) override {
    return traits_type::not_eof(c);
  }
  int sync() override {
    ++syncs;
    return 0;
  }
  int syncs = 0;
};

BOOST_AUTO_TEST_CASE(flushes_tied_stream) {
  SyncCounter tied_buffer;
  std::ostream tied(&tied_buffer);
  std::ostringstream out;
  out.tie(&tied);
  daily::FastOStream<std::ostream> fast(out);
  fast << 2;
  BOOST_TEST(tied_buffer.syncs == 1);
  BOOST_TEST(out.str() == "2");
}

BOOST_AUTO_TEST_CASE(unitbuf_flushes) {
  SyncCounter buffer;
  std::ostream out(&buffer);
  out << std::unitbuf;
  daily::FastOStream<std::ostream> fast(out);
  fast << 2 << 'c';
  BOOST_TEST(buffer.syncs == 2);
}

BOOST_AUTO_TEST_CASE(full_buffer_sets_badbit) {
  // A stream buffer that accepts nothing.
  struct FullBuffer : std::streambuf {
    int_type overflow(int_type) override {
      return traits_type::eof();
    }
  } buffer;
  std::ostream out(&buffer);
  daily::FastOStream<std::ostream> fast(out);
  fast << 42;
  BOOST_TEST(out.bad());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(daily_fast_istream)

BOOST_AUTO_TEST_CASE(integers) {
  std::istringstream in("42 -7 123456789012 65535 18446744073709551615");
  daily::FastIStream<std::istream> fast(in);
  int i = 0;
  short s = 0;
  long long ll = 0;
  unsigned short us = 0;
  unsigned long long ull = 0;
  fast >> i >> s >> ll >> us >> ull;
  BOOST_TEST(!in.fail());
  BOOST_TEST(i == 42);
  BOOST_TEST(s == -7);
  BOOST_TEST(ll == 123456789012LL);
  BOOST_TEST(us == 65535);
  BOOST_TEST(ull == std::numeric_limits<unsigned long long>::max());
}

BOOST_AUTO_TEST_CASE(reals_keep_precision) {
  std::istringstream in("0.1 0.1 2.5");
  daily::FastIStream<std::istream> fast(in);
  double d = 0;
  long double ld = 0;
  float f = 0;
  fast >> d >> ld >> f;
  BOOST_TEST(!in.fail());
  BOOST_TEST(d == 0.1);
#ifdef __MINGW32__
  // MinGW's long double is wider than double, but its C runtime parses long
  // doubles at double precision.
  BOOST_TEST(ld == static_cast<long double>(0.1));
#else
  BOOST_TEST(ld == 0.1L);
#endif
  BOOST_TEST(f == 2.5f);
}

BOOST_AUTO_TEST_CASE(strings) {
  std::istringstream in("word 5");
  daily::FastIStream<std::istream> fast(in);
  std::string s;
  int i = 0;
  fast >> s >> i;
  BOOST_TEST(s == "word");
  BOOST_TEST(i == 5);
}

BOOST_AUTO_TEST_CASE(leading_plus) {
  std::istringstream in("+5 +2.5");
  daily::FastIStream<std::istream> fast(in);
  int i = 0;
  double d = 0;
  fast >> i >> d;
  BOOST_TEST(!in.fail());
  BOOST_TEST(i == 5);
  BOOST_TEST(d == 2.5);
}

BOOST_AUTO_TEST_CASE(out_of_range_sets_failbit) {
  std::istringstream in("99999999999");
  daily::FastIStream<std::istream> fast(in);
  int i = 1;
  fast >> i;
  BOOST_TEST(in.fail());
  BOOST_TEST(i == 0);
}

BOOST_AUTO_TEST_CASE(invalid_sets_failbit) {
  std::istringstream in("abc");
  daily::FastIStream<std::istream> fast(in);
  int i = 0;
  fast >> i;
  BOOST_TEST(in.fail());
}

BOOST_AUTO_TEST_CASE(overlong_token_sets_failbit) {
  std::istringstream in(std::string(100, '1'));
  daily::FastIStream<std::istream> fast(in);
  long long i = 0;
  fast >> i;
  BOOST_TEST(in.fail());
}

BOOST_AUTO_TEST_CASE(end_of_input_sets_failbit) {
  std::istringstream in("1");
  daily::FastIStream<std::istream> fast(in);
  int a = 0;
  int b = 0;
  fast >> a;
  BOOST_TEST(!in.fail());
  BOOST_TEST(a == 1);
  fast >> b;
  BOOST_TEST(in.fail());
}

BOOST_AUTO_TEST_CASE(failed_stream_reads_nothing) {
  std::istringstream in("5");
  in.setstate(std::ios_base::failbit);
  daily::FastIStream<std::istream> fast(in);
  int i = 1;
  fast >> i;
  BOOST_TEST(i == 0);
  in.clear();
  fast >> i;
  BOOST_TEST(i == 5);
}

BOOST_AUTO_TEST_CASE(mixed_whitespace) {
  std::istringstream in(" \t\n12\r\n\v\f34 ");
  daily::FastIStream<std::istream> fast(in);
  int a = 0;
  int b = 0;
  fast >> a >> b;
  BOOST_TEST(!in.fail());
  BOOST_TEST(a == 12);
  BOOST_TEST(b == 34);
  BOOST_TEST(!in.eof());
}

BOOST_AUTO_TEST_CASE(reads_across_buffer_refills) {
  // A stream buffer that hands out one character at a time, so every token
  // spans several underflows.
  struct OneCharBuffer : std::streambuf {
    explicit OneCharBuffer(std::string text)
        : text_(std::move(text)) {
    }
    int_type underflow() override {
      if(pos_ == text_.size()) {
        return traits_type::eof();
      }
      setg(&text_[pos_], &text_[pos_], &text_[pos_] + 1);
      ++pos_;
      return traits_type::to_int_type(*gptr());
    }
    std::string text_;
    std::size_t pos_ = 0;
  } buffer("123 -4.5");
  std::istream in(&buffer);
  daily::FastIStream<std::istream> fast(in);
  int i = 0;
  double d = 0;
  fast >> i >> d;
  BOOST_TEST(!in.fail());
  BOOST_TEST(i == 123);
  BOOST_TEST(d == -4.5);
  BOOST_TEST(in.eof());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(daily_fast_locale)

BOOST_AUTO_TEST_CASE(formats_numbers) {
  std::ostringstream out;
  out.imbue(daily::generate_fast_locale("C"));
  out << 42 << ' ' << -7L << ' ' << 7u << ' ' << 1.5 << ' ' << true;
  BOOST_TEST(out.str() == "42 -7 7 1.5 1");
}

BOOST_AUTO_TEST_CASE(parses_numbers) {
  std::istringstream in("42 -7 7 0.1 2.5");
  in.imbue(daily::generate_fast_locale("C"));
  long l = 0;
  int i = 0;
  unsigned int u = 0;
  double d = 0;
  float f = 0;
  in >> l >> i >> u >> d >> f;
  BOOST_TEST(!in.fail());
  BOOST_TEST(l == 42);
  BOOST_TEST(i == -7);
  BOOST_TEST(u == 7u);
  BOOST_TEST(d == 0.1);
  BOOST_TEST(f == 2.5f);
  BOOST_TEST(in.eof());
}

BOOST_AUTO_TEST_CASE(invalid_sets_failbit) {
  std::istringstream in("abc");
  in.imbue(daily::generate_fast_locale("C"));
  long l = 0;
  in >> l;
  BOOST_TEST(in.fail());
}

BOOST_AUTO_TEST_SUITE_END()
