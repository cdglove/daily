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
#ifndef DAILY_SUPPORT_BOOSTSERIALIZATION_HPP
#define DAILY_SUPPORT_BOOSTSERIALIZATION_HPP
#pragma once

#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c_tag.hpp>
#include <boost/serialization/level.hpp>
#include <boost/serialization/tracking.hpp>

#include "daily/utility/CompactOptional.hpp"

namespace daily {

// Serialises a compact_optional as its value, with the sentinel for no
// value, so that no value reads back as no value.
template <typename Archive, typename T, T Sentinel>
void serialize(Archive& ar, compact_optional<T, Sentinel>& value, unsigned /*version*/) {
  if constexpr(Archive::is_loading::value) {
    T raw{};
    ar & raw;
    value = raw;
  }
  else {
    T raw = value.value_or(Sentinel);
    ar & raw;
  }
}

} // namespace daily

// By default Boost serialises a class with class information and tracking,
// which costs a call through its serializer for each value and doubles the
// time to serialise a struct of them. A compact_optional is a plain value, so
// it's written as one.
namespace boost::serialization {

template <typename T, T Sentinel>
struct implementation_level_impl<const daily::compact_optional<T, Sentinel>> {
  typedef mpl::integral_c_tag tag;
  typedef mpl::int_<object_serializable> type;
  BOOST_STATIC_CONSTANT(int, value = type::value);
};

template <typename T, T Sentinel>
struct tracking_level<daily::compact_optional<T, Sentinel>> {
  typedef mpl::integral_c_tag tag;
  typedef mpl::int_<track_never> type;
  BOOST_STATIC_CONSTANT(int, value = type::value);
};

} // namespace boost::serialization

#endif // DAILY_SUPPORT_BOOSTSERIALIZATION_HPP
