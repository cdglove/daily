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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>

#include "daily/utility/CompactOptional.hpp"

namespace std {
inline ostream& operator<<(ostream& os, nullopt_t) {
  return os << "nullopt";
}
} // namespace std

namespace {

// Compile-time checks
static_assert(sizeof(daily::compact_optional<int, -1>) == sizeof(int));
static_assert(sizeof(daily::compact_optional_max<std::size_t>) == sizeof(std::size_t));
static_assert(std::is_trivially_copyable_v<daily::compact_optional<int, -1>>);
static_assert(std::is_trivially_destructible_v<daily::compact_optional<int, -1>>);

constexpr bool test_constexpr() {
  daily::compact_optional<int, -1> opt;
  if(opt.has_value())
    return false;
  opt = 10;
  if(!opt.has_value() || *opt != 10)
    return false;
  if(opt.value_or(5) != 10)
    return false;
  opt.reset();
  if(opt.has_value() || opt.value_or(5) != 5)
    return false;
  return true;
}
static_assert(test_constexpr());

} // namespace

BOOST_AUTO_TEST_SUITE(daily_compact_optional)

BOOST_AUTO_TEST_CASE(default_construction_is_empty) {
  daily::compact_optional<int, -1> opt;
  BOOST_TEST(!opt.has_value());
  BOOST_TEST(!opt);
  BOOST_TEST(opt == std::nullopt);
  BOOST_TEST(std::nullopt == opt);
  BOOST_TEST(opt.sentinel() == -1);
}

BOOST_AUTO_TEST_CASE(nullopt_construction_is_empty) {
  daily::compact_optional<int, -1> opt(std::nullopt);
  BOOST_TEST(!opt.has_value());
  BOOST_TEST(opt == std::nullopt);
}

BOOST_AUTO_TEST_CASE(value_construction_has_value) {
  daily::compact_optional<int, -1> opt(42);
  BOOST_TEST(opt.has_value());
  BOOST_TEST(bool(opt));
  BOOST_TEST(*opt == 42);
  BOOST_TEST(opt.value() == 42);
  BOOST_TEST(opt == 42);
  BOOST_TEST(42 == opt);
  BOOST_TEST(opt != std::nullopt);
}

BOOST_AUTO_TEST_CASE(sentinel_value_construction_is_empty) {
  daily::compact_optional<int, -1> opt(-1);
  BOOST_TEST(!opt.has_value());
  BOOST_TEST(opt == std::nullopt);
}

BOOST_AUTO_TEST_CASE(copy_and_move_semantics) {
  daily::compact_optional<int, -1> a(100);
  daily::compact_optional<int, -1> b = a;
  BOOST_TEST(b.has_value());
  BOOST_TEST(*b == 100);

  daily::compact_optional<int, -1> c = std::move(a);
  BOOST_TEST(c.has_value());
  BOOST_TEST(*c == 100);

  daily::compact_optional<int, -1> empty;
  daily::compact_optional<int, -1> empty_copy = empty;
  BOOST_TEST(!empty_copy.has_value());
}

BOOST_AUTO_TEST_CASE(assignment_and_reset) {
  daily::compact_optional<int, -1> opt;
  BOOST_TEST(!opt.has_value());

  opt = 50;
  BOOST_TEST(opt.has_value());
  BOOST_TEST(*opt == 50);

  opt = std::nullopt;
  BOOST_TEST(!opt.has_value());

  opt = 60;
  BOOST_TEST(opt.has_value());
  BOOST_TEST(*opt == 60);

  opt.reset();
  BOOST_TEST(!opt.has_value());

  opt.emplace(70);
  BOOST_TEST(opt.has_value());
  BOOST_TEST(*opt == 70);
}

BOOST_AUTO_TEST_CASE(converting_constructor_and_assignment) {
  daily::compact_optional<int, -1> src(42);
  daily::compact_optional<long, -99> dst = src;
  BOOST_TEST(dst.has_value());
  BOOST_TEST(*dst == 42L);

  src.reset();
  dst = src;
  BOOST_TEST(!dst.has_value());
}

BOOST_AUTO_TEST_CASE(value_or) {
  daily::compact_optional<int, -1> empty;
  BOOST_TEST(empty.value_or(123) == 123);

  daily::compact_optional<int, -1> engaged(456);
  BOOST_TEST(engaged.value_or(123) == 456);
}

BOOST_AUTO_TEST_CASE(value_throws_bad_optional_access_when_empty) {
  daily::compact_optional<int, -1> opt;
  BOOST_CHECK_THROW((void)opt.value(), std::bad_optional_access);

  opt = 10;
  BOOST_CHECK_NO_THROW((void)opt.value());
}

BOOST_AUTO_TEST_CASE(aliases) {
  daily::compact_optional_max<std::size_t> opt_max;
  BOOST_TEST(!opt_max.has_value());
  opt_max = 1024u;
  BOOST_TEST(opt_max.has_value());
  BOOST_TEST(*opt_max == 1024u);
  BOOST_TEST(opt_max.sentinel() == std::numeric_limits<std::size_t>::max());

  daily::compact_optional_min<int> opt_min;
  BOOST_TEST(!opt_min.has_value());
  opt_min = -10;
  BOOST_TEST(opt_min.has_value());
  BOOST_TEST(*opt_min == -10);
  BOOST_TEST(opt_min.sentinel() == std::numeric_limits<int>::min());

  daily::compact_optional_lowest<int> opt_lowest;
  BOOST_TEST(!opt_lowest.has_value());
  BOOST_TEST(opt_lowest.sentinel() == std::numeric_limits<int>::lowest());

  daily::compact_optional_zero<int> opt_zero;
  BOOST_TEST(!opt_zero.has_value());
  opt_zero = 5;
  BOOST_TEST(opt_zero.has_value());
  BOOST_TEST(*opt_zero == 5);
  opt_zero = 0;
  BOOST_TEST(!opt_zero.has_value());
}

BOOST_AUTO_TEST_CASE(comparisons_with_compact_optional) {
  daily::compact_optional<int, -1> empty1;
  daily::compact_optional<int, -1> empty2;
  daily::compact_optional<int, -1> val10(10);
  daily::compact_optional<int, -1> val20(20);
  daily::compact_optional<int, -1> val10_again(10);

  // equality
  BOOST_TEST(empty1 == empty2);
  BOOST_TEST(val10 == val10_again);
  BOOST_TEST(val10 != val20);
  BOOST_TEST(empty1 != val10);
  BOOST_TEST(val10 != empty1);

  // ordering
  BOOST_TEST(empty1 < val10);
  BOOST_TEST(val10 > empty1);
  BOOST_TEST(empty1 <= empty2);
  BOOST_TEST(val10 <= val10_again);
  BOOST_TEST(val10 < val20);
  BOOST_TEST(val20 > val10);
}

BOOST_AUTO_TEST_CASE(comparisons_with_nullopt) {
  daily::compact_optional<int, -1> empty;
  daily::compact_optional<int, -1> val(5);

  BOOST_TEST(empty == std::nullopt);
  BOOST_TEST(std::nullopt == empty);
  BOOST_TEST(val != std::nullopt);
  BOOST_TEST(std::nullopt != val);

  BOOST_TEST(empty <= std::nullopt);
  BOOST_TEST(empty >= std::nullopt);
  BOOST_TEST(val > std::nullopt);
  BOOST_TEST(std::nullopt < val);
}

BOOST_AUTO_TEST_CASE(comparisons_with_raw_value) {
  daily::compact_optional<int, -1> empty;
  daily::compact_optional<int, -1> val(10);

  BOOST_TEST(val == 10);
  BOOST_TEST(10 == val);
  BOOST_TEST(val != 20);
  BOOST_TEST(20 != val);

  BOOST_TEST(val < 20);
  BOOST_TEST(val > 5);
  BOOST_TEST(empty < 10);
  BOOST_TEST(10 > empty);
  BOOST_TEST(empty != 10);
}

BOOST_AUTO_TEST_CASE(swap_operation) {
  daily::compact_optional<int, -1> a(10);
  daily::compact_optional<int, -1> b;

  a.swap(b);
  BOOST_TEST(!a.has_value());
  BOOST_TEST(b.has_value());
  BOOST_TEST(*b == 10);

  using std::swap;
  swap(a, b);
  BOOST_TEST(a.has_value());
  BOOST_TEST(*a == 10);
  BOOST_TEST(!b.has_value());
}

BOOST_AUTO_TEST_CASE(std_hash_support) {
  daily::compact_optional<int, -1> empty;
  daily::compact_optional<int, -1> engaged(42);

  std::hash<daily::compact_optional<int, -1>> hasher;
  BOOST_TEST(hasher(empty) == 0u);
  BOOST_TEST(hasher(engaged) == std::hash<int>{}(42));
}

BOOST_AUTO_TEST_SUITE_END()
