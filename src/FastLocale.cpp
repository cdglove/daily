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
#include "daily/fast_iostream/FastLocale.hpp"
#include "daily/fast_iostream/detail/Format.hpp"

namespace daily {
namespace {

class FastNumPut : public std::num_put<char> {
 protected:
  using base_type = std::num_put<char>;

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, bool v) const override {
    return base_type::do_put(out, str, fill, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, long v) const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, long long v)
      const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, unsigned long v)
      const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(
      iter_type out,
      std::ios_base& str,
      char_type fill,
      unsigned long long v) const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, double v) const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, long double v)
      const override {
    return detail::write_number(out, v);
  }

  iter_type do_put(iter_type out, std::ios_base& str, char_type fill, const void* v)
      const override {
    return base_type::do_put(out, str, fill, v);
  }
};

// Parses one token from [in, end) into v, setting err as std::num_get
// does.
template <typename T>
std::istreambuf_iterator<char> get_number(
    std::istreambuf_iterator<char> in,
    std::istreambuf_iterator<char> end,
    std::ios_base::iostate& err,
    T& v) {
  detail::FixedInputString<32> s(in, end);
  if(!detail::parse_number(s, v)) {
    err |= std::ios_base::failbit;
  }
  if(s.reached_end) {
    err |= std::ios_base::eofbit;
  }
  return in;
}

class FastNumGet : public std::num_get<char> {
 protected:
  using base_type = std::num_get<char>;

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      bool& v) const override {
    return base_type::do_get(in, end, str, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      long& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      long long& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      unsigned short& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      unsigned int& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      unsigned long& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      unsigned long long& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      float& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      double& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      long double& v) const override {
    return get_number(in, end, err, v);
  }

  iter_type do_get(
      iter_type in,
      iter_type end,
      std::ios_base& str,
      std::ios_base::iostate& err,
      void*& v) const override {
    return base_type::do_get(in, end, str, err, v);
  }
};

} // namespace

std::locale generate_fast_locale(char const* loc_name) {
  std::locale r = std::locale(loc_name);

  r = std::locale(r, new FastNumPut);
  r = std::locale(r, new FastNumGet);

  return r;
}

} // namespace daily
