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
#include <benchmark/benchmark.h>

#include <charconv>
#include <cstddef>
#include <format>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <random>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include "daily/fast_iostream/FastIStream.hpp"
#include "daily/fast_iostream/FastLocale.hpp"
#include "daily/fast_iostream/FastOStream.hpp"

// Compares number formatting and parsing through std::ostream and
// std::istream, the same streams imbued with daily::generate_fast_locale,
// FastOStream and FastIStream, std::format_to, and std::to_chars and
// std::from_chars called directly as a lower bound.
//
// Each benchmark converts kCount values. Output goes to a fixed buffer that
// is rewound rather than grown, so memory allocation isn't measured.
namespace {

constexpr std::size_t kCount = 10'000;

// Writes into a fixed buffer, rewinding when it fills.
class FixedBuffer : public std::streambuf {
 public:
  FixedBuffer()
      : data_(1 << 20) {
    rewind();
  }

  void rewind() {
    setp(data_.data(), data_.data() + data_.size());
  }

 protected:
  int_type overflow(int_type c) override {
    rewind();
    if(!traits_type::eq_int_type(c, traits_type::eof())) {
      sputc(traits_type::to_char_type(c));
    }
    return traits_type::not_eof(c);
  }

 private:
  std::vector<char> data_;
};

template <typename T>
std::vector<T> make_values() {
  std::mt19937_64 rng(42);
  std::vector<T> values(kCount);
  for(T& v : values) {
    if constexpr(std::is_floating_point_v<T>) {
      v = std::uniform_real_distribution<T>(-1e6, 1e6)(rng);
    }
    else {
      v = std::uniform_int_distribution<T>(
          std::numeric_limits<T>::min(),
          std::numeric_limits<T>::max())(rng);
    }
  }
  return values;
}

// The values as space separated text in shortest round trip form.
template <typename T>
std::string make_text() {
  std::string text;
  for(T v : make_values<T>()) {
    char buf[64];
    text.append(buf, std::to_chars(buf, buf + sizeof(buf), v).ptr);
    text += ' ';
  }
  return text;
}

std::locale named_locale() {
  try {
    return std::locale("en_US.UTF-8");
  }
  catch(std::runtime_error const&) {
    return std::locale::classic();
  }
}

template <typename T>
void set_items(benchmark::State& state) {
  state.SetItemsProcessed(state.iterations() * kCount);
}

// Formatting

template <typename T>
void bm_format_ostream(benchmark::State& state, std::locale const& loc) {
  auto values = make_values<T>();
  FixedBuffer buffer;
  std::ostream out(&buffer);
  out.imbue(loc);
  for(auto _ : state) {
    for(T v : values) {
      out << v << ' ';
    }
    buffer.rewind();
  }
  set_items<T>(state);
}

template <typename T>
void bm_format_ostream_classic(benchmark::State& state) {
  bm_format_ostream<T>(state, std::locale::classic());
}

template <typename T>
void bm_format_ostream_named(benchmark::State& state) {
  bm_format_ostream<T>(state, named_locale());
}

template <typename T>
void bm_format_ostream_round_trip(benchmark::State& state) {
  auto values = make_values<T>();
  FixedBuffer buffer;
  std::ostream out(&buffer);
  out << std::setprecision(std::numeric_limits<T>::max_digits10);
  for(auto _ : state) {
    for(T v : values) {
      out << v << ' ';
    }
    buffer.rewind();
  }
  set_items<T>(state);
}

template <typename T>
void bm_format_ostream_fast_locale(benchmark::State& state) {
  bm_format_ostream<T>(state, daily::generate_fast_locale("C"));
}

template <typename T>
void bm_format_fast_ostream(benchmark::State& state) {
  auto values = make_values<T>();
  FixedBuffer buffer;
  std::ostream out(&buffer);
  daily::FastOStream<std::ostream> fast(out);
  for(auto _ : state) {
    for(T v : values) {
      fast << v << ' ';
    }
    buffer.rewind();
  }
  set_items<T>(state);
}

template <typename T>
void bm_format_std_format(benchmark::State& state) {
  auto values = make_values<T>();
  std::vector<char> buffer(kCount * 32);
  for(auto _ : state) {
    char* out = buffer.data();
    for(T v : values) {
      out = std::format_to(out, "{} ", v);
    }
    benchmark::DoNotOptimize(out);
  }
  set_items<T>(state);
}

template <typename T>
void bm_format_to_chars(benchmark::State& state) {
  auto values = make_values<T>();
  std::vector<char> buffer(kCount * 32);
  for(auto _ : state) {
    char* out = buffer.data();
    for(T v : values) {
      out = std::to_chars(out, out + 32, v).ptr;
      *out++ = ' ';
    }
    benchmark::DoNotOptimize(out);
  }
  set_items<T>(state);
}

// Parsing

template <typename T>
void bm_parse_istream(benchmark::State& state, std::locale const& loc) {
  std::string text = make_text<T>();
  for(auto _ : state) {
    std::istringstream in(text);
    in.imbue(loc);
    T v;
    for(std::size_t i = 0; i < kCount; ++i) {
      in >> v;
    }
    benchmark::DoNotOptimize(v);
  }
  set_items<T>(state);
}

template <typename T>
void bm_parse_istream_classic(benchmark::State& state) {
  bm_parse_istream<T>(state, std::locale::classic());
}

template <typename T>
void bm_parse_istream_named(benchmark::State& state) {
  bm_parse_istream<T>(state, named_locale());
}

template <typename T>
void bm_parse_istream_fast_locale(benchmark::State& state) {
  bm_parse_istream<T>(state, daily::generate_fast_locale("C"));
}

template <typename T>
void bm_parse_fast_istream(benchmark::State& state) {
  std::string text = make_text<T>();
  for(auto _ : state) {
    std::istringstream in(text);
    daily::FastIStream<std::istream> fast(in);
    T v;
    for(std::size_t i = 0; i < kCount; ++i) {
      fast >> v;
    }
    benchmark::DoNotOptimize(v);
  }
  set_items<T>(state);
}

template <typename T>
void bm_parse_from_chars(benchmark::State& state) {
  std::string text = make_text<T>();
  for(auto _ : state) {
    char const* p = text.data();
    char const* end = p + text.size();
    T v;
    for(std::size_t i = 0; i < kCount; ++i) {
      p = std::from_chars(p, end, v).ptr + 1;
    }
    benchmark::DoNotOptimize(v);
  }
  set_items<T>(state);
}

} // namespace

#define DAILY_BENCH_TYPES(function)                                            \
  BENCHMARK(function<int>);                                                    \
  BENCHMARK(function<long long>);                                              \
  BENCHMARK(function<double>)

DAILY_BENCH_TYPES(bm_format_ostream_classic);
DAILY_BENCH_TYPES(bm_format_ostream_named);
BENCHMARK(bm_format_ostream_round_trip<double>);
DAILY_BENCH_TYPES(bm_format_ostream_fast_locale);
DAILY_BENCH_TYPES(bm_format_fast_ostream);
DAILY_BENCH_TYPES(bm_format_std_format);
DAILY_BENCH_TYPES(bm_format_to_chars);

DAILY_BENCH_TYPES(bm_parse_istream_classic);
DAILY_BENCH_TYPES(bm_parse_istream_named);
DAILY_BENCH_TYPES(bm_parse_istream_fast_locale);
DAILY_BENCH_TYPES(bm_parse_fast_istream);
DAILY_BENCH_TYPES(bm_parse_from_chars);

BENCHMARK_MAIN();
