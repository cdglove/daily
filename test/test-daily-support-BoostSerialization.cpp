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
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <ctime>
#include <sstream>

#include "daily/support/BoostSerialization.hpp"
#include "daily/utility/CompactOptional.hpp"

namespace {

struct Values {
  daily::compact_optional_max<std::uint64_t> size;
  daily::compact_optional_max<std::time_t> modified;
  daily::compact_optional<int, -1> count;

  template <typename Archive>
  void serialize(Archive& ar, unsigned /*version*/) {
    ar & size;
    ar & modified;
    ar & count;
  }
};

} // namespace

BOOST_AUTO_TEST_SUITE(daily_support_boost_serialization)

BOOST_AUTO_TEST_CASE(round_trips_values_and_no_value) {
  Values sent;
  sent.modified = -86400; // Values below zero aren't the sentinel.
  sent.count = 0;

  std::stringstream wire;
  {
    boost::archive::binary_oarchive out(wire, boost::archive::no_header);
    out << sent;
  }
  Values received;
  received.size = 5;
  {
    boost::archive::binary_iarchive in(wire, boost::archive::no_header);
    in >> received;
  }

  BOOST_CHECK(!received.size.has_value());
  BOOST_REQUIRE(received.modified.has_value());
  BOOST_CHECK_EQUAL(*received.modified, -86400);
  BOOST_REQUIRE(received.count.has_value());
  BOOST_CHECK_EQUAL(*received.count, 0);
}

BOOST_AUTO_TEST_CASE(written_as_plain_values) {
  // Without class information or tracking, each is just its value.
  daily::compact_optional_max<std::uint64_t> const value = 7;
  std::stringstream wire;
  {
    boost::archive::binary_oarchive out(wire, boost::archive::no_header);
    out << value;
    out << value;
  }
  BOOST_CHECK_EQUAL(wire.str().size(), 2 * sizeof(std::uint64_t));
}

BOOST_AUTO_TEST_SUITE_END()
