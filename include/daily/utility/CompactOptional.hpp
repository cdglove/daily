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
#ifndef DAILY_UTILITY_COMPACTOPTIONAL_HPP
#define DAILY_UTILITY_COMPACTOPTIONAL_HPP
#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>

namespace daily {

// An optional-like vocabulary type that uses an in-band sentinel value
// to represent the empty state, achieving sizeof(compact_optional<T, Sentinel>)
// == sizeof(T).
template <typename T, T Sentinel>
class compact_optional {
 public:
  using value_type = T;

  static constexpr T sentinel() noexcept {
    return Sentinel;
  }

  // Constructors
  constexpr compact_optional() noexcept
      : value_(Sentinel) {
  }
  // Converts implicitly, as std::optional does.
  // NOLINTNEXTLINE(misc-explicit-constructor)
  constexpr compact_optional(std::nullopt_t) noexcept
      : value_(Sentinel) {
  }

  // Converts implicitly, as std::optional does.
  // NOLINTNEXTLINE(misc-explicit-constructor)
  constexpr compact_optional(T const& value) noexcept(
      std::is_nothrow_copy_constructible_v<T>)
      : value_(value) {
  }

  // Converts implicitly, as std::optional does.
  // NOLINTNEXTLINE(misc-explicit-constructor)
  constexpr compact_optional(T&& value) noexcept(
      std::is_nothrow_move_constructible_v<T>)
      : value_(std::move(value)) {
  }

  template <typename U>
    requires(
        !std::is_same_v<std::remove_cvref_t<U>, compact_optional> &&
        !std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t> &&
        !std::is_same_v<std::remove_cvref_t<U>, T> && std::is_constructible_v<T, U>)
  constexpr explicit(!std::is_convertible_v<U, T>) compact_optional(U&& value)
      : value_(std::forward<U>(value)) {
  }

  template <typename U, U OtherSentinel>
    requires(
        !std::is_same_v<compact_optional<U, OtherSentinel>, compact_optional> &&
        std::is_constructible_v<T, U const&>)
  constexpr explicit(!std::is_convertible_v<U const&, T>)
      compact_optional(compact_optional<U, OtherSentinel> const& other)
      : value_(other.has_value() ? static_cast<T>(*other) : Sentinel) {
  }

  template <typename U, U OtherSentinel>
    requires(
        !std::is_same_v<compact_optional<U, OtherSentinel>, compact_optional> &&
        std::is_constructible_v<T, U>)
  constexpr explicit(!std::is_convertible_v<U, T>)
      compact_optional(compact_optional<U, OtherSentinel>&& other)
      : value_(other.has_value() ? static_cast<T>(std::move(*other)) : Sentinel) {
  }

  constexpr compact_optional(compact_optional const&) = default;
  constexpr compact_optional(compact_optional&&) noexcept = default;
  constexpr compact_optional& operator=(compact_optional const&) = default;
  constexpr compact_optional& operator=(compact_optional&&) noexcept = default;
  ~compact_optional() = default;

  // Assignment
  constexpr compact_optional& operator=(std::nullopt_t) noexcept {
    reset();
    return *this;
  }

  template <typename U = T>
    requires(
        !std::is_same_v<std::remove_cvref_t<U>, compact_optional> &&
        !std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t> &&
        std::is_assignable_v<T&, U>)
  constexpr compact_optional& operator=(U&& value) {
    value_ = std::forward<U>(value);
    return *this;
  }

  template <typename U, U OtherSentinel>
    requires(
        !std::is_same_v<compact_optional<U, OtherSentinel>, compact_optional> &&
        std::is_assignable_v<T&, U const&>)
  constexpr compact_optional& operator=(compact_optional<U, OtherSentinel> const& other) {
    if(other.has_value()) {
      value_ = *other;
    }
    else {
      reset();
    }
    return *this;
  }

  template <typename U, U OtherSentinel>
    requires(
        !std::is_same_v<compact_optional<U, OtherSentinel>, compact_optional> &&
        std::is_assignable_v<T&, U>)
  constexpr compact_optional& operator=(compact_optional<U, OtherSentinel>&& other) {
    if(other.has_value()) {
      value_ = std::move(*other);
    }
    else {
      reset();
    }
    return *this;
  }

  // Modifiers
  constexpr void reset() noexcept {
    value_ = Sentinel;
  }

  template <typename... Args>
    requires std::is_constructible_v<T, Args...>
  constexpr T& emplace(Args&&... args) {
    value_ = T(std::forward<Args>(args)...);
    return value_;
  }

  constexpr void swap(compact_optional& other) noexcept(
      std::is_nothrow_swappable_v<T>) {
    std::swap(value_, other.value_);
  }

  // Observers
  [[nodiscard]] constexpr bool has_value() const noexcept {
    return value_ != Sentinel;
  }

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return has_value();
  }

  [[nodiscard]] constexpr T const& operator*() const& noexcept {
    assert(has_value());
    return value_;
  }

  [[nodiscard]] constexpr T& operator*() & noexcept {
    assert(has_value());
    return value_;
  }

  [[nodiscard]] constexpr T const&& operator*() const&& noexcept {
    assert(has_value());
    return std::move(value_);
  }

  [[nodiscard]] constexpr T&& operator*() && noexcept {
    assert(has_value());
    return std::move(value_);
  }

  [[nodiscard]] constexpr T const* operator->() const noexcept {
    assert(has_value());
    return &value_;
  }

  [[nodiscard]] constexpr T* operator->() noexcept {
    assert(has_value());
    return &value_;
  }

  [[nodiscard]] constexpr T const& value() const& {
    if(!has_value()) {
      throw std::bad_optional_access();
    }
    return value_;
  }

  [[nodiscard]] constexpr T& value() & {
    if(!has_value()) {
      throw std::bad_optional_access();
    }
    return value_;
  }

  [[nodiscard]] constexpr T const&& value() const&& {
    if(!has_value()) {
      throw std::bad_optional_access();
    }
    return std::move(value_);
  }

  [[nodiscard]] constexpr T&& value() && {
    if(!has_value()) {
      throw std::bad_optional_access();
    }
    return std::move(value_);
  }

  template <typename U>
  [[nodiscard]] constexpr T value_or(U&& default_value) const& {
    return has_value() ? value_ : static_cast<T>(std::forward<U>(default_value));
  }

  template <typename U>
  [[nodiscard]] constexpr T value_or(U&& default_value) && {
    return has_value() ? std::move(value_)
                       : static_cast<T>(std::forward<U>(default_value));
  }

  // Comparisons between compact_optional instances
  template <typename U, U OtherSentinel>
    requires std::equality_comparable_with<T, U>
  friend constexpr bool operator==(
      compact_optional const& lhs,
      compact_optional<U, OtherSentinel> const& rhs) noexcept {
    bool const lhs_has = lhs.has_value();
    bool const rhs_has = rhs.has_value();
    if(lhs_has != rhs_has) {
      return false;
    }
    if(!lhs_has) {
      return true;
    }
    return *lhs == *rhs;
  }

  template <typename U, U OtherSentinel>
    requires std::three_way_comparable_with<T, U>
  friend constexpr auto operator<=>(
      compact_optional const& lhs,
      compact_optional<U, OtherSentinel> const& rhs) {
    bool const lhs_has = lhs.has_value();
    bool const rhs_has = rhs.has_value();
    if(!lhs_has && !rhs_has) {
      return std::strong_ordering::equal;
    }
    if(!lhs_has) {
      return std::strong_ordering::less;
    }
    if(!rhs_has) {
      return std::strong_ordering::greater;
    }
    return *lhs <=> *rhs;
  }

  // Comparison with nullopt
  friend constexpr bool operator==(compact_optional const& opt, std::nullopt_t) noexcept {
    return !opt.has_value();
  }

  friend constexpr auto operator<=>(compact_optional const& opt, std::nullopt_t) noexcept {
    return opt.has_value() <=> false;
  }

  // Comparison with value
  friend constexpr bool operator==(compact_optional const& opt, T const& val) noexcept {
    return opt.has_value() ? *opt == val : false;
  }

  friend constexpr auto operator<=>(compact_optional const& opt, T const& val)
    requires std::three_way_comparable<T>
  {
    return opt.has_value() ? *opt <=> val : std::strong_ordering::less;
  }

  // Stream output
  template <typename CharT, typename Traits>
  friend std::basic_ostream<CharT, Traits>& operator<<(
      std::basic_ostream<CharT, Traits>& os,
      compact_optional const& opt) {
    if(opt.has_value()) {
      os << *opt;
    }
    else {
      os << "{nullopt}";
    }
    return os;
  }

 private:
  T value_ = Sentinel;
};

// Aliases for common numeric limits sentinels
template <typename T>
using compact_optional_max = compact_optional<T, std::numeric_limits<T>::max()>;

template <typename T>
using compact_optional_min = compact_optional<T, std::numeric_limits<T>::min()>;

template <typename T>
using compact_optional_lowest = compact_optional<T, std::numeric_limits<T>::lowest()>;

template <typename T>
using compact_optional_zero = compact_optional<T, T{0}>;

// Non-member swap
template <typename T, T Sentinel>
constexpr void swap(
    compact_optional<T, Sentinel>& lhs,
    compact_optional<T, Sentinel>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

} // namespace daily

namespace std {

template <typename T, T Sentinel>
struct hash<daily::compact_optional<T, Sentinel>> {
  std::size_t operator()(daily::compact_optional<T, Sentinel> const& opt) const noexcept {
    return opt.has_value() ? std::hash<T>{}(*opt) : static_cast<std::size_t>(0);
  }
};

} // namespace std

#endif // DAILY_UTILITY_COMPACTOPTIONAL_HPP
