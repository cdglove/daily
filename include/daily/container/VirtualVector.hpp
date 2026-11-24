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
#ifndef DAILY_CONTAINER_VIRTUALVECTOR_HPP
#define DAILY_CONTAINER_VIRTUALVECTOR_HPP
#pragma once

#include "daily/memory/VirtualMemory.hpp"

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/core/noncopyable.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

namespace daily {

// A contiguous sequence stored in a VirtualRegion. It grows by committing
// more of the region in place, so elements never move: pointers and
// references to them stay valid until the element is removed, and growing
// never copies. The region fixes where the elements are and how many there
// can be.
//
// The reservation the region came from must outlive the vector.
template <typename T>
class VirtualVector : boost::noncopyable {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = T const&;
  using pointer = T*;
  using const_pointer = T const*;
  using iterator = T*;
  using const_iterator = T const*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Growing commits at least this much at a time, so that a growing vector
  // rarely calls into the system.
  static constexpr std::size_t kMinCommit = std::size_t{2} << 20;

  VirtualVector() = default;

  explicit VirtualVector(VirtualRegion region)
      : region_(region)
      , data_(reinterpret_cast<T*>(region.data())) {
    BOOST_ASSERT(reinterpret_cast<std::uintptr_t>(data_) % alignof(T) == 0);
  }

  VirtualVector(VirtualVector&& other) noexcept
      : region_(std::exchange(other.region_, VirtualRegion()))
      , data_(std::exchange(other.data_, nullptr))
      , size_(std::exchange(other.size_, 0))
      , committed_(std::exchange(other.committed_, 0)) {
  }

  VirtualVector& operator=(VirtualVector&& other) noexcept {
    if(this != &other) {
      reset();
      region_ = std::exchange(other.region_, VirtualRegion());
      data_ = std::exchange(other.data_, nullptr);
      size_ = std::exchange(other.size_, 0);
      committed_ = std::exchange(other.committed_, 0);
    }
    return *this;
  }

  // Destroys the elements and decommits the region, so another vector can
  // use it.
  ~VirtualVector() {
    reset();
  }

  VirtualRegion region() const {
    return region_;
  }

  T* data() {
    return data_;
  }
  T const* data() const {
    return data_;
  }

  size_type size() const {
    return size_;
  }

  bool empty() const {
    return size_ == 0;
  }

  // The number of elements the committed pages hold.
  size_type capacity() const {
    return committed_ / sizeof(T);
  }

  // The number of elements the region holds.
  size_type max_size() const {
    return region_.size() / sizeof(T);
  }

  iterator begin() {
    return data_;
  }
  iterator end() {
    return data_ + size_;
  }
  const_iterator begin() const {
    return data_;
  }
  const_iterator end() const {
    return data_ + size_;
  }
  const_iterator cbegin() const {
    return begin();
  }
  const_iterator cend() const {
    return end();
  }
  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }
  reverse_iterator rend() {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  T& operator[](size_type i) {
    BOOST_ASSERT(i < size_);
    return data_[i];
  }
  T const& operator[](size_type i) const {
    BOOST_ASSERT(i < size_);
    return data_[i];
  }

  T& front() {
    BOOST_ASSERT(!empty());
    return data_[0];
  }
  T const& front() const {
    BOOST_ASSERT(!empty());
    return data_[0];
  }
  T& back() {
    BOOST_ASSERT(!empty());
    return data_[size_ - 1];
  }
  T const& back() const {
    BOOST_ASSERT(!empty());
    return data_[size_ - 1];
  }

  // Commits pages for at least n elements. Throws std::length_error if n is
  // more than max_size(), or std::bad_alloc if the pages can't be committed.
  void reserve(size_type n) {
    if(n > capacity()) {
      commit_for(n);
    }
  }

  // Since elements don't move, value may refer to one of this vector's own.
  void push_back(T const& value) {
    emplace_back(value);
  }

  void push_back(T&& value) {
    emplace_back(std::move(value));
  }

  template <typename... Args>
  T& emplace_back(Args&&... args) {
    if(size_ == capacity()) {
      commit_for(size_ + 1);
    }
    T* p = std::construct_at(data_ + size_, std::forward<Args>(args)...);
    ++size_;
    return *p;
  }

  void pop_back() {
    BOOST_ASSERT(!empty());
    --size_;
    std::destroy_at(data_ + size_);
  }

  // New elements are value-initialised, as std::vector's are.
  void resize(size_type n) {
    grow_to(n, [](T* p) { std::construct_at(p); });
  }

  void resize(size_type n, T const& value) {
    grow_to(n, [&](T* p) { std::construct_at(p, value); });
  }

  // Keeps the pages committed, as std::vector keeps its capacity.
  void clear() {
    truncate(0);
  }

  // Decommits the pages past the last element.
  void shrink_to_fit() {
    std::size_t const keep = VirtualMemory::round_to_page(size_ * sizeof(T));
    if(keep < committed_) {
      region_.decommit(keep, committed_ - keep);
      committed_ = keep;
    }
  }

  void swap(VirtualVector& other) noexcept {
    std::swap(region_, other.region_);
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(committed_, other.committed_);
  }

  friend void swap(VirtualVector& a, VirtualVector& b) noexcept {
    a.swap(b);
  }

 private:
  // Commits enough pages for n elements, growing by at least half the
  // committed size and kMinCommit, up to the end of the region.
  void commit_for(size_type n) {
    if(n > max_size()) {
      throw std::length_error("VirtualVector exceeds its region");
    }
    std::size_t const needed = VirtualMemory::round_to_page(n * sizeof(T));
    std::size_t const grown = VirtualMemory::round_to_page(
        std::max(committed_ + committed_ / 2, committed_ + kMinCommit));
    std::size_t const target = std::min(std::max(needed, grown), region_.size());
    region_.commit(committed_, target - committed_);
    committed_ = target;
  }

  // Constructs elements up to n with construct, or destroys those past it.
  // If a construction throws, the elements it added are destroyed again.
  template <typename Construct>
  void grow_to(size_type n, Construct&& construct) {
    if(n <= size_) {
      truncate(n);
      return;
    }
    reserve(n);
    size_type const old_size = size_;
    try {
      for(; size_ < n; ++size_) {
        construct(data_ + size_);
      }
    }
    catch(...) {
      truncate(old_size);
      throw;
    }
  }

  void truncate(size_type n) {
    std::destroy(data_ + n, data_ + size_);
    size_ = n;
  }

  void reset() {
    truncate(0);
    if(committed_ != 0) {
      region_.decommit(0, committed_);
      committed_ = 0;
    }
  }

  VirtualRegion region_;
  T* data_ = nullptr;
  size_type size_ = 0;
  // Committed bytes from the start of the region.
  std::size_t committed_ = 0;
};

} // namespace daily

#endif // DAILY_CONTAINER_VIRTUALVECTOR_HPP
