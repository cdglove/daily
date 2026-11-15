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
#ifndef DAILY_CONTAINER_SEGMENTEDSEQUENCE_HPP
#define DAILY_CONTAINER_SEGMENTEDSEQUENCE_HPP
#pragma once

#include "daily/detail/Prefetch.hpp"

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

namespace daily {

// A sequence stored as a list of segments, each a std::vector of up to
// SplitThreshold elements. Inserting or erasing only shifts elements within
// one segment. A prefix sum of segment sizes gives random access: finding the
// segment holding an index is a binary search over the segments, and each
// insert or erase adjusts the prefix sum for the segments after it.
//
// A full segment splits in two on insert. Segments other than the last are
// kept at least MergeThreshold long by merging with or taking elements from
// a neighbour. Smaller segments make inserting and erasing cheaper, but mean
// more segments to search and to adjust the prefix sum for.
//
// The default of 2K elements was measured with bench-fsdb-ordinal-container
// and bench-fsdb on containers of 100K to 10M elements. The best size grows
// with the container: about 1K for 100K elements, 2K for 1M, and 2K to 4K
// for 10M. 2K was within about 10% of the best at every size, and much
// faster than larger segments at inserting and erasing.
template <typename T, std::size_t SplitThreshold = 2 * 1024, std::size_t MergeThreshold = SplitThreshold / 4>
class SegmentedSequence
    : public boost::stl_interfaces::sequence_container_interface<
          SegmentedSequence<T, SplitThreshold, MergeThreshold>,
          boost::stl_interfaces::element_layout::discontiguous> {
  // A segment that has to take elements from a neighbour is left with at
  // least half of a full segment, which must not be below the merge
  // threshold.
  static_assert(MergeThreshold > 0 && MergeThreshold * 2 <= SplitThreshold);

 private:
  using base_type = boost::stl_interfaces::sequence_container_interface<
      SegmentedSequence<T, SplitThreshold, MergeThreshold>,
      boost::stl_interfaces::element_layout::discontiguous>;

  template <bool IsConst>
  class iterator_impl
      : public boost::stl_interfaces::iterator_interface<
            iterator_impl<IsConst>,
            std::random_access_iterator_tag,
            T,
            std::conditional_t<IsConst, T const&, T&>,
            std::conditional_t<IsConst, T const*, T*>> {
   private:
    using base_type = boost::stl_interfaces::iterator_interface<
        iterator_impl<IsConst>,
        std::random_access_iterator_tag,
        T,
        std::conditional_t<IsConst, T const&, T&>,
        std::conditional_t<IsConst, T const*, T*>>;
    using container_iterator = std::conditional_t<
        IsConst,
        typename std::vector<std::vector<T>>::const_iterator,
        typename std::vector<std::vector<T>>::iterator>;
    using segment_iterator = std::conditional_t<
        IsConst,
        typename std::vector<T>::const_iterator,
        typename std::vector<T>::iterator>;
    using owner_ptr =
        std::conditional_t<IsConst, SegmentedSequence const*, SegmentedSequence*>;
    friend class iterator_impl<!IsConst>;

   public:
    using difference_type = typename base_type::difference_type;

    iterator_impl() = default;
    iterator_impl(container_iterator c, segment_iterator i, owner_ptr owner)
        : c_(c)
        , i_(i)
        , owner_(owner) {
      validate();
    }

    template <bool OtherConst, std::enable_if_t<IsConst && !OtherConst, int> = 0>
    iterator_impl(iterator_impl<OtherConst> const& other)
        : c_(other.c_)
        , i_(other.i_)
        , owner_(other.owner_) {
      validate();
    }

    iterator_impl& operator++() {
      ++i_;
      if(i_ == c_->end()) [[unlikely]] {
        if(c_ != owner_->t_.end() - 1) [[likely]] {
          ++c_;
          i_ = c_->begin();
        }
      }
      validate();
      return *this;
    }

    // Postfix increment and decrement are defined here rather than taken
    // from iterator_interface with a using-declaration, which would also
    // bring in its prefix versions. Those are constrained, so overload
    // resolution would prefer them to these, and they go through +=.
    iterator_impl operator++(int) {
      auto result = *this;
      ++*this;
      return result;
    }

    iterator_impl& operator--() {
      if(i_ == c_->begin()) [[unlikely]] {
        if(c_ != owner_->t_.begin()) [[likely]] {
          --c_;
          i_ = c_->end();
          --i_;
        }
      }
      else {
        --i_;
      }
      validate();
      return *this;
    }

    iterator_impl operator--(int) {
      auto result = *this;
      --*this;
      return result;
    }

    // iterator_interface provides all of the comparisons, and != in terms
    // of ==, but implements the others by subtracting, which needs the
    // global index of both iterators. Comparing positions is cheaper.
    // i_ is compared first because it differs on almost every comparison,
    // while c_ is still checked in case the end of the last segment's
    // allocation is adjacent in memory to the start of another segment.
    bool operator==(iterator_impl const& other) const {
      return i_ == other.i_ && c_ == other.c_;
    }

    bool operator<(iterator_impl const& other) const {
      if(c_ == other.c_) {
        return i_ < other.i_;
      }
      return c_ < other.c_;
    }

    bool operator<=(iterator_impl const& other) const {
      return !(other < *this);
    }

    bool operator>(iterator_impl const& other) const {
      return other < *this;
    }

    bool operator>=(iterator_impl const& other) const {
      return !(*this < other);
    }

    std::conditional_t<IsConst, T const&, T&> operator*() const {
      return *this->i_;
    }

    iterator_impl& operator+=(difference_type n) {
      auto const offset = std::distance(c_->begin(), i_) + n;
      auto const segment_size = static_cast<difference_type>(c_->size());
      bool const last_segment = c_ == owner_->t_.end() - 1;
      if(offset >= 0 &&
         (offset < segment_size || (offset == segment_size && last_segment))) {
        // Staying within the segment.
        i_ += n;
      }
      else {
        auto segment = static_cast<std::size_t>(
            std::distance(owner_->t_.begin(), c_));
        seek(owner_->starts_[segment] + offset, segment);
      }
      validate();
      return *this;
    }

    // A friend rather than a member so that it doesn't hide the operator-
    // for subtracting a distance that iterator_interface provides, which is
    // a member in some configurations of stl_interfaces.
    friend difference_type operator-(iterator_impl const& a, iterator_impl const& b) {
      return a.global_index() - b.global_index();
    }

   protected:
    friend std::ostream& operator<<(std::ostream& os, iterator_impl const& it) {
      os << std::addressof(*it.c_) << " " << std::addressof(*it.i_);
      return os;
    }

    difference_type global_index() const {
      auto segment = static_cast<std::size_t>(std::distance(owner_->t_.begin(), c_));
      return static_cast<difference_type>(owner_->starts_[segment]) +
             std::distance(c_->begin(), i_);
    }

    // Moves to the element at the given index in the whole container.
    // from_segment is the segment the iterator is moving from.
    void seek(difference_type index, std::size_t from_segment) {
      BOOST_ASSERT(index >= 0);
      BOOST_ASSERT(index <= static_cast<difference_type>(owner_->size_));
      auto& t = owner_->t_;
      if(index == static_cast<difference_type>(owner_->size_)) {
        c_ = t.end() - 1;
        i_ = c_->end();
        return;
      }
      auto const segment = owner_->find_segment(index, from_segment);
      c_ = t.begin() + static_cast<difference_type>(segment);
      i_ = c_->begin() +
           (index - static_cast<difference_type>(owner_->starts_[segment]));
    }

    void validate() const {
      // We always keep one element around so we should never leak the
      // table end iterator.
      BOOST_ASSERT(c_ != owner_->t_.end());
      // We should never leak the end iterator of a segment unless
      // except in the case that this is the last segment.
      BOOST_ASSERT(i_ != c_->end() || c_ == owner_->t_.end() - 1);
    }

    friend class SegmentedSequence;
    container_iterator c_;
    segment_iterator i_;
    owner_ptr owner_ = nullptr;
  };

 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = T const*;
  using reference = value_type&;
  using const_reference = value_type const&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;
  using reverse_iterator = boost::stl_interfaces::reverse_iterator<iterator>;
  using const_reverse_iterator = boost::stl_interfaces::reverse_iterator<const_iterator>;

  BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(iterator, std::random_access_iterator);
  BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(const_iterator, std::random_access_iterator);

  BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
      iterator,
      std::random_access_iterator_tag,
      std::random_access_iterator,
      T,
      T&,
      T*,
      std::ptrdiff_t);
  BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
      const_iterator,
      std::random_access_iterator_tag,
      std::random_access_iterator,
      T,
      T const&,
      T const*,
      std::ptrdiff_t);

  SegmentedSequence() {
    clear();
  }

  SegmentedSequence(size_type n, T const& value)
      : SegmentedSequence() {
    assign(n, value);
  }

  template <std::input_iterator InputIterator>
  SegmentedSequence(InputIterator first, InputIterator last)
      : SegmentedSequence() {
    append(first, last);
  }

  SegmentedSequence(std::initializer_list<T> list)
      : SegmentedSequence(list.begin(), list.end()) {
  }

  SegmentedSequence(SegmentedSequence const& other) = default;
  SegmentedSequence& operator=(SegmentedSequence const& other) = default;

  // Moving leaves other empty rather than without any segments.
  SegmentedSequence(SegmentedSequence&& other) noexcept
      : t_(std::move(other.t_))
      , starts_(std::move(other.starts_))
      , size_(other.size_) {
    other.clear();
  }

  SegmentedSequence& operator=(SegmentedSequence&& other) noexcept {
    t_ = std::move(other.t_);
    starts_ = std::move(other.starts_);
    size_ = other.size_;
    other.clear();
    return *this;
  }

  SegmentedSequence& operator=(std::initializer_list<T> list) {
    assign(list);
    return *this;
  }

  // Cheaper than the version in terms of erase().
  void clear() {
    t_.clear();
    // Always keep one around so we don't need to check for it.
    t_.resize(1);
    starts_.assign(1, 0);
    size_ = 0;
  }

  void shrink_to_fit() {
    t_.shrink_to_fit();
    starts_.shrink_to_fit();
    for(auto& v : t_) {
      v.shrink_to_fit();
    }
  }

  // Returns the first position in [begin, end) for which pred is false,
  // where pred is true for a prefix of the range, like std::partition_point.
  // Allows searching by something other than a T.
  template <typename Pred>
  iterator partition_point(const_iterator begin, const_iterator end, Pred pred) {
    return to_mutable(partition_point_impl(begin, end, pred));
  }

  template <typename Pred>
  const_iterator partition_point(const_iterator begin, const_iterator end, Pred pred) const {
    return partition_point_impl(begin, end, pred);
  }

  template <typename Compare>
  iterator lower_bound(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) {
    return to_mutable(lower_bound_impl(begin, end, value, compare));
  }

  template <typename Compare>
  iterator upper_bound(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) {
    return to_mutable(upper_bound_impl(begin, end, value, compare));
  }

  template <typename Compare>
  const_iterator lower_bound(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) const {
    return lower_bound_impl(begin, end, value, compare);
  }

  template <typename Compare>
  const_iterator upper_bound(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) const {
    return upper_bound_impl(begin, end, value, compare);
  }

  iterator rotate(iterator first, iterator middle, iterator last) {
    if(first == middle) {
      return last;
    }
    if(middle == last) {
      return first;
    }

    // Fast path for rotation within a single segment.
    if(first.c_ == last.c_) {
      return iterator(first.c_, std::rotate(first.i_, middle.i_, last.i_), this);
    }

    // Split segments so that first, middle and last each start a segment,
    // then rotate whole segments. Split from the back so that splitting
    // doesn't move the positions that are still to be split.
    auto ifirst = iterator_to_index(first);
    auto imiddle = iterator_to_index(middle);
    auto ilast = iterator_to_index(last);

    auto last_segment = split_before(ilast);

    auto middle_segment = split_before(imiddle);
    if(middle_segment != imiddle.segment) {
      ++last_segment;
    }

    auto first_segment = split_before(ifirst);
    if(first_segment != ifirst.segment) {
      ++middle_segment;
      ++last_segment;
    }

    std::rotate(
        t_.begin() + static_cast<difference_type>(first_segment),
        t_.begin() + static_cast<difference_type>(middle_segment),
        t_.begin() + static_cast<difference_type>(last_segment));
    rotate_starts(first_segment, middle_segment, last_segment);

    // The element that was at first now starts the segment after the
    // segments that were moved down.
    auto const result_segment = first_segment + (last_segment - middle_segment);
    auto const result = static_cast<difference_type>(starts_[result_segment]);

    // Splitting can leave small segments either side of each cut. Fix them
    // from the back so that merging doesn't move the segments still to fix.
    std::size_t const cuts[] = {
        last_segment,
        last_segment - 1,
        result_segment,
        result_segment - 1,
        first_segment,
        first_segment == 0 ? 0 : first_segment - 1};
    for(auto segment : cuts) {
      if(segment < t_.size()) {
        fix_segment_size(segment);
      }
    }
    return iterator_at(result, std::min(result_segment, t_.size() - 1));
  }

  // sequence_container_interface provides these, but implements assigning
  // a range by assigning over the existing elements one at a time.
  template <std::input_iterator InputIterator>
  void assign(InputIterator first, InputIterator last) {
    clear();
    append(first, last);
  }

  void assign(size_type n, T const& value) {
    clear();
    this->insert(this->cend(), n, value);
  }

  void assign(std::initializer_list<T> list) {
    clear();
    append(list.begin(), list.end());
  }

  void swap(SegmentedSequence& other) noexcept {
    std::swap(t_, other.t_);
    std::swap(starts_, other.starts_);
    std::swap(size_, other.size_);
  }

  // Direct indexing avoids constructing begin() and touching t_.front() on
  // every access as sequence_container_interface's *(begin() + n) does.
  reference operator[](size_type n) {
    BOOST_ASSERT(n < size_);
    auto const segment = find_segment(static_cast<difference_type>(n), 0);
    return t_[segment][n - starts_[segment]];
  }

  const_reference operator[](size_type n) const {
    BOOST_ASSERT(n < size_);
    auto const segment = find_segment(static_cast<difference_type>(n), 0);
    return t_[segment][n - starts_[segment]];
  }

  // sequence_container_interface provides the rest of the container
  // interface in terms of these members. It would also provide the const
  // begin() and end() from the non-const ones, but these hide them.
  iterator begin() {
    return iterator(t_.begin(), t_.front().begin(), this);
  }

  iterator end() {
    return iterator(t_.end() - 1, t_.back().end(), this);
  }

  const_iterator begin() const {
    return const_iterator(t_.begin(), t_.front().begin(), this);
  }

  const_iterator end() const {
    return const_iterator(t_.end() - 1, t_.back().end(), this);
  }

  // When t_.back() already has spare capacity, its size is also below
  // kSplitThreshold and inserting cannot reallocate or invalidate arguments
  // that alias existing elements. Otherwise the element is constructed
  // before making room for it.
  template <typename... Args>
  reference emplace_back(Args&&... args) {
    BOOST_ASSERT(size_ < kMaxSize);
    if(auto& back = t_.back(); back.size() < back.capacity()) [[likely]] {
      ++size_;
      return back.emplace_back(std::forward<Args>(args)...);
    }
    T value(std::forward<Args>(args)...);
    if(t_.back().size() == kSplitThreshold) {
      add_segment_at_end();
    }
    reserve_for_insert(t_.back(), 1);
    ++size_;
    return t_.back().emplace_back(std::move(value));
  }

  // Cheaper than the version in terms of erase().
  void pop_back() {
    BOOST_ASSERT(!empty());
    t_.back().pop_back();
    if(t_.back().empty() && t_.size() > 1) {
      t_.pop_back();
      starts_.pop_back();
    }
    else {
      maybe_shrink(t_.back());
    }
    --size_;
  }

  // Cheaper than the versions in terms of begin() and end().
  bool empty() const {
    return size() == 0;
  }

  std::size_t size() const {
    return size_;
  }

  std::size_t capacity() const {
    std::size_t cap = 0;
    for(auto const& v : t_) {
      cap += v.capacity();
    }
    return cap;
  }

  // The capacity of each segment, for tests and diagnostics.
  std::vector<std::size_t> segment_capacities() const {
    std::vector<std::size_t> capacities;
    capacities.reserve(t_.size());
    for(auto const& segment : t_) {
      capacities.push_back(segment.capacity());
    }
    return capacities;
  }

  // The number of elements in each segment, for tests and diagnostics.
  std::vector<std::size_t> segment_sizes() const {
    std::vector<std::size_t> sizes;
    sizes.reserve(t_.size());
    for(auto const& segment : t_) {
      sizes.push_back(segment.size());
    }
    return sizes;
  }

  template <typename... Args>
  iterator emplace(const_iterator cpos, Args&&... args) {
    if(cpos == this->cend()) {
      emplace_back(std::forward<Args>(args)...);
      return iterator(t_.end() - 1, t_.back().end() - 1, this);
    }

    BOOST_ASSERT(size_ < kMaxSize);
    T value(std::forward<Args>(args)...);
    auto const segment = static_cast<std::size_t>(
        std::distance(t_.cbegin(), cpos.c_));
    auto& seg = t_[segment];
    auto const offset = std::distance(seg.cbegin(), cpos.i_);

    if(seg.size() == kSplitThreshold) {
      return split_and_insert(segment, offset, std::move(value));
    }

    if(seg.size() == seg.capacity()) {
      // Reallocate and insert in one pass rather than moving the tail in
      // reserve() and immediately shifting it again in insert().
      std::vector<T> grown;
      grown.reserve(capacity_for(seg.size() + 1));
      auto const pos_it = seg.begin() + offset;
      grown.insert(
          grown.end(),
          std::make_move_iterator(seg.begin()),
          std::make_move_iterator(pos_it));
      grown.push_back(std::move(value));
      grown.insert(
          grown.end(),
          std::make_move_iterator(pos_it),
          std::make_move_iterator(seg.end()));
      seg = std::move(grown);
    }
    else {
      seg.insert(seg.begin() + offset, std::move(value));
    }

    ++size_;
    add_to_starts(segment + 1, 1);
    auto const c = t_.begin() + static_cast<difference_type>(segment);
    return iterator(c, c->begin() + offset, this);
  }

  // Declaring insert hides the versions of it that
  // sequence_container_interface provides in terms of emplace and inserting
  // a range. Its version for an initializer_list is constrained such that
  // it's never used, so that's defined here.
  using base_type::insert;

  // Inserts [first, last), which must not be in this container. Elements
  // that fit in the segment at pos are inserted into it. Otherwise the
  // segment is split at pos and the elements go in new segments between the
  // two halves.
  template <std::input_iterator InputIterator>
  iterator insert(const_iterator pos, InputIterator first, InputIterator last) {
    auto const idx = iterator_to_index(pos);
    auto const index = static_cast<difference_type>(
        starts_[idx.segment] + idx.index);
    if(pos == this->cend()) {
      append(first, last);
      return iterator_at(index, idx.segment);
    }
    if constexpr(!std::forward_iterator<InputIterator>) {
      // The elements can only be counted by reading them.
      std::vector<T> elements(first, last);
      return insert(
          pos,
          std::make_move_iterator(elements.begin()),
          std::make_move_iterator(elements.end()));
    }
    else {
      auto const count = static_cast<std::size_t>(std::distance(first, last));
      BOOST_ASSERT(size_ + count <= kMaxSize);
      auto& segment = t_[idx.segment];
      if(segment.size() + count <= kSplitThreshold) {
        auto const offset = static_cast<difference_type>(idx.index);
        if(segment.size() + count > segment.capacity()) {
          std::vector<T> grown;
          grown.reserve(capacity_for(segment.size() + count));
          auto const split = segment.begin() + offset;
          grown.insert(
              grown.end(),
              std::make_move_iterator(segment.begin()),
              std::make_move_iterator(split));
          grown.insert(grown.end(), first, last);
          grown.insert(
              grown.end(),
              std::make_move_iterator(split),
              std::make_move_iterator(segment.end()));
          segment = std::move(grown);
        }
        else {
          segment.insert(segment.begin() + offset, first, last);
        }
        size_ += count;
        add_to_starts(idx.segment + 1, static_cast<offset_type>(count));
        auto const c = t_.begin() + static_cast<difference_type>(idx.segment);
        return iterator(c, c->begin() + offset, this);
      }

      // Spread the elements evenly over as few segments as hold them, which
      // leaves each at least half full if there's more than one.
      auto const num_segments = (count + kSplitThreshold - 1) / kSplitThreshold;
      std::vector<std::vector<T>> segments;
      std::vector<offset_type> starts;
      segments.reserve(num_segments);
      starts.reserve(num_segments);
      auto start = static_cast<offset_type>(index);
      for(std::size_t i = 0; i < num_segments; ++i) {
        auto const n = count / num_segments + (i < count % num_segments ? 1 : 0);
        auto next = std::next(first, static_cast<difference_type>(n));
        segments.push_back(make_segment(first, next));
        starts.push_back(start);
        first = next;
        start += static_cast<offset_type>(n);
      }

      auto const at = split_before(idx);
      add_to_starts(at, static_cast<offset_type>(count));
      t_.insert(
          t_.begin() + static_cast<difference_type>(at),
          std::make_move_iterator(segments.begin()),
          std::make_move_iterator(segments.end()));
      starts_.insert(
          starts_.begin() + static_cast<difference_type>(at),
          starts.begin(),
          starts.end());
      size_ += count;

      // Splitting can leave small segments either side of the new ones, and
      // a single new segment can be small. Fix them from the back so that
      // merging doesn't move the segments still to fix.
      auto const after = at + num_segments;
      std::size_t const cuts[] = {after, after - 1, at, at == 0 ? 0 : at - 1};
      for(auto s : cuts) {
        if(s < t_.size()) {
          fix_segment_size(s);
        }
      }
      return iterator_at(index, std::min(at, t_.size() - 1));
    }
  }

  iterator insert(const_iterator pos, std::initializer_list<T> list) {
    return insert(pos, list.begin(), list.end());
  }

  // Cheaper than the version in terms of erasing a range, which also
  // doesn't return an iterator.
  iterator erase(const_iterator pos) {
    auto const segment = static_cast<std::size_t>(
        std::distance(t_.cbegin(), pos.c_));
    auto& seg = t_[segment];
    auto const offset = std::distance(seg.cbegin(), pos.i_);
    --size_;
    subtract_from_starts(segment + 1, 1);

    if(t_.size() == 1 || seg.size() > kMergeThreshold) [[likely]] {
      erase_within_segment(seg, offset, offset + 1);
      return iterator_after_in_place_erase(segment, offset);
    }

    // The element after the erased one takes its index.
    auto const index = static_cast<difference_type>(starts_[segment]) + offset;
    seg.erase(seg.begin() + offset);
    fix_segment_size(segment);
    return iterator_at(index, std::min(segment, t_.size() - 1));
  }

  iterator erase(const_iterator first, const_iterator last) {
    auto const f = iterator_to_index(first);
    auto const l = iterator_to_index(last);
    auto const index = static_cast<difference_type>(starts_[f.segment] + f.index);
    if(f.segment == l.segment) {
      auto const count = static_cast<offset_type>(l.index - f.index);
      if(count == 0) {
        return to_mutable(first);
      }
      size_ -= count;
      subtract_from_starts(f.segment + 1, count);
      auto& segment = t_[f.segment];
      if(t_.size() == 1 || segment.size() - count >= kMergeThreshold) {
        erase_within_segment(
            segment,
            static_cast<difference_type>(f.index),
            static_cast<difference_type>(l.index));
        return iterator_after_in_place_erase(
            f.segment,
            static_cast<difference_type>(f.index));
      }
      segment.erase(
          segment.begin() + static_cast<difference_type>(f.index),
          segment.begin() + static_cast<difference_type>(l.index));
      fix_segment_size(f.segment);
    }
    else {
      auto const end_index = static_cast<difference_type>(
          starts_[l.segment] + l.index);
      auto const count = static_cast<offset_type>(end_index - index);
      size_ -= count;
      // Erase the end of first's segment, the segments in between, and the
      // start of last's segment, which then starts at index.
      auto& head = t_[f.segment];
      auto& tail = t_[l.segment];
      head.erase(head.begin() + static_cast<difference_type>(f.index), head.end());
      tail.erase(tail.begin(), tail.begin() + static_cast<difference_type>(l.index));
      auto const between = static_cast<difference_type>(f.segment + 1);
      auto const end_between = static_cast<difference_type>(l.segment);
      t_.erase(t_.begin() + between, t_.begin() + end_between);
      starts_.erase(starts_.begin() + between, starts_.begin() + end_between);
      starts_[f.segment + 1] = static_cast<offset_type>(index);
      subtract_from_starts(f.segment + 2, count);
      // Either side can be left small or empty. Fix the later one first so
      // that merging doesn't move the other.
      fix_segment_size(f.segment + 1);
      fix_segment_size(f.segment);
    }
    return iterator_at(index, std::min(f.segment, t_.size() - 1));
  }

  static constexpr std::size_t kSplitThreshold = SplitThreshold;
  static constexpr std::size_t kMergeThreshold = MergeThreshold;

 private:
  // Segment starts are 32 bit, which halves the memory traffic when
  // adjusting them and lets the compiler process twice as many per vector
  // instruction.
  using offset_type = std::uint32_t;
  static constexpr std::size_t kMaxSize = std::numeric_limits<offset_type>::max();

  struct Index {
    std::size_t segment;
    std::size_t index;
  };

  // Returns the segment holding index, which is the last segment starting
  // at or before it because segments are never empty. Checks from_segment
  // and its immediate left neighbour first so that hints after insert/erase
  // resolve immediately; otherwise guesses by assuming average segment size,
  // gallops out to bracket the target, and binary searches the bracket.
  std::size_t find_segment(difference_type index, std::size_t from_segment) const {
    auto const target = static_cast<offset_type>(index);
    if(starts_[from_segment] <= target) {
      if(from_segment + 1 == starts_.size() || target < starts_[from_segment + 1]) {
        return from_segment;
      }
    }
    else if(from_segment > 0 && starts_[from_segment - 1] <= target) {
      return from_segment - 1;
    }

    auto const num_segments = static_cast<std::int64_t>(starts_.size());
    auto const distance = static_cast<std::int64_t>(index) -
                          static_cast<std::int64_t>(starts_[from_segment]);
    auto const step_guess = (distance * num_segments) /
                            static_cast<std::int64_t>(size_);
    auto const guess = static_cast<std::size_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(from_segment) + step_guess,
        0,
        num_segments - 1));

    // Segments are close to the same size, so the guess is usually right or
    // one off. Check those with plain comparisons before galloping.
    auto const last = starts_.size() - 1;
    if(starts_[guess] <= target) {
      if(guess == last || target < starts_[guess + 1]) {
        return guess;
      }
      if(guess + 1 == last || target < starts_[guess + 2]) {
        return guess + 1;
      }
    }
    else if(guess > 0 && starts_[guess - 1] <= target) {
      return guess - 1;
    }

    // Find lo and hi such that starts_[lo] <= target and either hi is the
    // number of segments or starts_[hi] > target.
    std::size_t lo;
    std::size_t hi;
    std::size_t step = 1;
    if(starts_[guess] <= target) {
      lo = guess;
      while(true) {
        auto probe = lo + step;
        if(probe >= starts_.size()) {
          hi = starts_.size();
          break;
        }
        if(starts_[probe] > target) {
          hi = probe;
          break;
        }
        lo = probe;
        step *= 2;
      }
    }
    else {
      hi = guess;
      while(true) {
        if(step > hi) {
          // starts_[0] is 0, which is never greater than target.
          lo = 0;
          break;
        }
        auto probe = hi - step;
        if(starts_[probe] <= target) {
          lo = probe;
          break;
        }
        hi = probe;
        step *= 2;
      }
    }
    return static_cast<std::size_t>(
        std::upper_bound(
            starts_.begin() + static_cast<difference_type>(lo + 1),
            starts_.begin() + static_cast<difference_type>(hi),
            target) -
        starts_.begin() - 1);
  }

  // Adjust the starts of the segments from first on, after inserting or
  // erasing n elements before them. These loops are the cost of random
  // access, kept simple so that the compiler vectorises them.
  void add_to_starts(std::size_t first, offset_type n) {
    auto* it = starts_.data() + first;
    auto* const end = starts_.data() + starts_.size();
    for(; it != end; ++it) {
      *it += n;
    }
  }

  void subtract_from_starts(std::size_t first, offset_type n) {
    auto* it = starts_.data() + first;
    auto* const end = starts_.data() + starts_.size();
    for(; it != end; ++it) {
      *it -= n;
    }
  }

  // Updates starts_ after rotating segments [first, middle, last). The
  // segments that were in [middle, last) move down by the number of elements
  // in [first, middle) and the others move up by the number in
  // [middle, last). Rotating starts_ and then adjusting each part by a
  // constant vectorises, unlike recomputing each start from the previous one.
  void rotate_starts(std::size_t first, std::size_t middle, std::size_t last) {
    offset_type const first_start = starts_[first];
    offset_type const middle_start = starts_[middle];
    offset_type const last_start = last == starts_.size()
                                       ? static_cast<offset_type>(size_)
                                       : starts_[last];
    offset_type const moved_down = middle_start - first_start;
    offset_type const moved_up = last_start - middle_start;
    std::rotate(
        starts_.begin() + static_cast<difference_type>(first),
        starts_.begin() + static_cast<difference_type>(middle),
        starts_.begin() + static_cast<difference_type>(last));
    auto const boundary = starts_.begin() +
                          static_cast<difference_type>(first + (last - middle));
    for(auto it = starts_.begin() + static_cast<difference_type>(first);
        it != boundary;
        ++it) {
      *it -= moved_down;
    }
    for(auto it = boundary;
        it != starts_.begin() + static_cast<difference_type>(last);
        ++it) {
      *it += moved_up;
    }
  }

  // Called when the last segment is full, so the container is growing by
  // appending. Reserving a whole segment up front avoids growing it
  // gradually, and wastes at most part of one segment.
  void add_segment_at_end() {
    t_.emplace_back();
    t_.back().reserve(kSplitThreshold);
    starts_.push_back(static_cast<offset_type>(size_));
  }

  // Segment memory. Segments never have room for more than kSplitThreshold
  // elements, and their allocations are kept in proportion to their size:
  // they grow by about a quarter at a time, and are reallocated smaller when
  // erasing leaves them less than half full. Growing and shrinking both copy
  // the segment, but only after a number of inserts or erases in proportion
  // to its size, so the cost per element is constant.

  // The capacity to give a segment that will hold size elements.
  static std::size_t capacity_for(std::size_t size) {
    return std::min<std::size_t>(kSplitThreshold, size + size / 4 + 16);
  }

  // Makes room for count more elements in segment. std::vector would grow by
  // doubling, which can exceed a full segment.
  static void reserve_for_insert(std::vector<T>& segment, std::size_t count) {
    auto const needed = segment.size() + count;
    BOOST_ASSERT(needed <= static_cast<std::size_t>(kSplitThreshold));
    if(needed > segment.capacity()) {
      segment.reserve(capacity_for(needed));
    }
  }

  // Moves the elements of segment to a new allocation sized for them if it
  // is less than half full.
  static void maybe_shrink(std::vector<T>& segment) {
    if(segment.size() < segment.capacity() / 2) {
      segment = make_segment(
          std::make_move_iterator(segment.begin()),
          std::make_move_iterator(segment.end()));
    }
  }

  // Erases [first_offset, last_offset) from segment, reallocating in a
  // single pass if the resulting size falls below half of capacity.
  static void erase_within_segment(
      std::vector<T>& segment,
      difference_type first_offset,
      difference_type last_offset) {
    auto const new_size = segment.size() -
                          static_cast<std::size_t>(last_offset - first_offset);
    if(new_size < segment.capacity() / 2) {
      std::vector<T> shrunk;
      shrunk.reserve(capacity_for(new_size));
      shrunk.insert(
          shrunk.end(),
          std::make_move_iterator(segment.begin()),
          std::make_move_iterator(segment.begin() + first_offset));
      shrunk.insert(
          shrunk.end(),
          std::make_move_iterator(segment.begin() + last_offset),
          std::make_move_iterator(segment.end()));
      segment = std::move(shrunk);
    }
    else {
      segment.erase(segment.begin() + first_offset, segment.begin() + last_offset);
    }
  }

  iterator iterator_after_in_place_erase(std::size_t segment, difference_type offset) {
    auto c = t_.begin() + static_cast<difference_type>(segment);
    if(static_cast<std::size_t>(offset) == c->size() && segment + 1 < t_.size()) {
      ++c;
      return iterator(c, c->begin(), this);
    }
    return iterator(c, c->begin() + offset, this);
  }

  // Returns a segment holding [begin, end) with capacity to grow.
  template <typename Iterator>
  static std::vector<T> make_segment(Iterator begin, Iterator end) {
    std::vector<T> segment;
    segment.reserve(
        capacity_for(static_cast<std::size_t>(std::distance(begin, end))));
    segment.insert(segment.end(), begin, end);
    return segment;
  }

  // Appends [begin, end), filling segments the same way push_back does but
  // copying whole chunks at a time when possible.
  template <typename Iterator>
  void append(Iterator begin, Iterator end) {
    using category = typename std::iterator_traits<Iterator>::iterator_category;
    if constexpr(std::is_base_of_v<std::random_access_iterator_tag, category>) {
      BOOST_ASSERT(size_ + std::distance(begin, end) <= kMaxSize);
      while(begin != end) {
        if(t_.back().size() == kSplitThreshold) {
          add_segment_at_end();
        }
        auto& segment = t_.back();
        auto count = std::min<difference_type>(
            kSplitThreshold - segment.size(),
            std::distance(begin, end));
        reserve_for_insert(segment, static_cast<std::size_t>(count));
        segment.insert(segment.end(), begin, begin + count);
        begin += count;
        size_ += count;
      }
    }
    else {
      std::copy(begin, end, std::back_inserter(*this));
    }
  }

  template <typename Compare>
  const_iterator lower_bound_impl(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) const {
    return partition_point_impl(begin, end, [&](T const& x) {
      return compare(x, value);
    });
  }

  template <typename Compare>
  const_iterator upper_bound_impl(
      const_iterator begin,
      const_iterator end,
      T const& value,
      Compare compare) const {
    return partition_point_impl(begin, end, [&](T const& x) {
      return !compare(value, x);
    });
  }

  // Returns the first position in [first, last) for which pred is false,
  // where pred is true for a prefix of the range, like std::partition_point.
  // While each probe is compared, the memory address(it) for both possible
  // next probes is prefetched, which overlaps the cache misses of
  // successive probes when searching a large container.
  template <typename It, typename Pred, typename Address>
  static It partition_point(It first, It last, Pred pred, Address address) {
    auto len = last - first;
    while(len > 2) {
      auto const half = len / 2;
      // Whichever way this probe goes, the next one is the middle of the
      // remaining half.
      detail::prefetch(address(first + half / 2));
      detail::prefetch(address(first + half + 1 + (len - half - 1) / 2));
      auto const middle = first + half;
      bool const cond = static_cast<bool>(pred(*middle));
      first = cond ? middle + 1 : first;
      len = cond ? len - (half + 1) : half;
    }
    if(len == 2) {
      auto const middle = first + 1;
      detail::prefetch(address(first));
      if(pred(*middle)) {
        return middle + 1;
      }
      len = 1;
    }
    if(len == 1 && pred(*first)) {
      ++first;
    }
    return first;
  }

  // Two level search: find the segment, using the last element of each,
  // then search within it. Returns the first position in [begin, end) for
  // which pred is false, where pred is true for a prefix of the range.
  template <typename Pred>
  const_iterator
  partition_point_impl(const_iterator begin, const_iterator end, Pred pred) const {
    auto const element_address = [](T const* p) { return p; };
    if(begin.c_ == end.c_) {
      auto const* base = begin.c_->data();
      auto const* first_ptr = base + (begin.i_ - begin.c_->begin());
      auto const* last_ptr = base + (end.i_ - begin.c_->begin());
      auto const* needle_ptr =
          partition_point(first_ptr, last_ptr, pred, element_address);
      return const_iterator(begin.c_, begin.c_->begin() + (needle_ptr - base), this);
    }

    // Segments in the range are never empty.
    auto container = partition_point(
        begin.c_,
        end.c_,
        [&pred](std::vector<T> const& segment) { return pred(segment.back()); },
        [](auto segment) { return segment->data() + (segment->size() - 1); });

    auto const* base = container->data();
    auto const* begin_segment = container == begin.c_
                                    ? base + (begin.i_ - container->begin())
                                    : base;
    auto const* end_segment = container == end.c_
                                  ? base + (end.i_ - container->begin())
                                  : base + container->size();

    // The result is never the end of a segment other than the last: either
    // the segment was chosen because its last element fails pred, or it is
    // the segment holding end, which only ends a segment if it's end().
    auto const* needle_ptr =
        partition_point(begin_segment, end_segment, pred, element_address);
    auto const needle = container->begin() + (needle_ptr - base);
    BOOST_ASSERT(needle != container->end() || container == t_.end() - 1);
    return {container, needle, this};
  }

  // Splits the full segment at segment in two and inserts value at offset in
  // a single pass so that elements after offset are only moved once. Both
  // halves get new allocations with room to grow, rather than the first
  // keeping the full segment's allocation.
  iterator split_and_insert(std::size_t segment, difference_type offset, T&& value) {
    constexpr std::size_t half = kSplitThreshold / 2;
    auto& seg = t_[segment];
    auto const middle = seg.begin() + static_cast<difference_type>(half);
    auto const pos_it = seg.begin() + offset;

    if(static_cast<std::size_t>(offset) < half) {
      auto target = make_segment(
          std::make_move_iterator(middle),
          std::make_move_iterator(seg.end()));
      std::vector<T> head;
      head.reserve(capacity_for(half));
      reserve_for_insert(head, half + 1);
      head.insert(
          head.end(),
          std::make_move_iterator(seg.begin()),
          std::make_move_iterator(pos_it));
      head.push_back(std::move(value));
      head.insert(
          head.end(),
          std::make_move_iterator(pos_it),
          std::make_move_iterator(middle));
      seg = std::move(head);

      starts_.insert(
          starts_.begin() + static_cast<difference_type>(segment + 1),
          starts_[segment] + static_cast<offset_type>(half + 1));
      t_.insert(
          t_.begin() + static_cast<difference_type>(segment + 1),
          std::move(target));
      ++size_;
      add_to_starts(segment + 2, 1);
      auto const c = t_.begin() + static_cast<difference_type>(segment);
      return iterator(c, c->begin() + offset, this);
    }
    else {
      auto const tail_offset = offset - static_cast<difference_type>(half);
      std::vector<T> target;
      target.reserve(capacity_for(kSplitThreshold - half));
      reserve_for_insert(target, kSplitThreshold - half + 1);
      target.insert(
          target.end(),
          std::make_move_iterator(middle),
          std::make_move_iterator(pos_it));
      target.push_back(std::move(value));
      target.insert(
          target.end(),
          std::make_move_iterator(pos_it),
          std::make_move_iterator(seg.end()));
      seg = make_segment(
          std::make_move_iterator(seg.begin()),
          std::make_move_iterator(middle));

      starts_.insert(
          starts_.begin() + static_cast<difference_type>(segment + 1),
          starts_[segment] + static_cast<offset_type>(half));
      auto const new_c = t_.insert(
          t_.begin() + static_cast<difference_type>(segment + 1),
          std::move(target));
      ++size_;
      add_to_starts(segment + 2, 1);
      return iterator(new_c, new_c->begin() + tail_offset, this);
    }
  }

  // Keeps segments at least kMergeThreshold long, other than the last
  // segment, which push_back fills, and the only segment. Called when
  // segment may have become too small. Merges it with its smaller neighbour
  // or, if the result would be too big to fit in a segment, moves elements
  // over from that neighbour so that both are at least half full.
  void fix_segment_size(std::size_t segment) {
    while(t_.size() > 1 && t_[segment].size() < kMergeThreshold) {
      if(t_[segment].empty()) {
        t_.erase(t_.begin() + static_cast<difference_type>(segment));
        starts_.erase(starts_.begin() + static_cast<difference_type>(segment));
        return;
      }

      bool const has_left = segment > 0;
      bool const has_right = segment + 1 < t_.size();
      auto const size_l = has_left ? t_[segment - 1].size() : 0;
      auto const size_r = has_right ? t_[segment + 1].size() : 0;
      bool const use_left = has_left && (!has_right || size_l < size_r);
      auto const neighbour = use_left ? segment - 1 : segment + 1;

      if(t_[segment].size() + t_[neighbour].size() < kSplitThreshold) {
        // Merge the right hand segment of the pair into the left.
        auto const left = std::min(segment, neighbour);
        move_contents_from_and_erase(
            t_.begin() + static_cast<difference_type>(left + 1),
            t_.begin() + static_cast<difference_type>(left));
        segment = left;
        continue;
      }

      auto const count = static_cast<difference_type>(
          (t_[neighbour].size() - t_[segment].size()) / 2);
      auto const ucount = static_cast<std::size_t>(count);
      auto& from = t_[neighbour];
      auto& to = t_[segment];
      if(use_left) {
        // Move the end of the left neighbour to the front of this segment.
        if(to.size() + ucount > to.capacity()) {
          std::vector<T> grown;
          grown.reserve(capacity_for(to.size() + ucount));
          grown.insert(
              grown.end(),
              std::make_move_iterator(from.end() - count),
              std::make_move_iterator(from.end()));
          grown.insert(
              grown.end(),
              std::make_move_iterator(to.begin()),
              std::make_move_iterator(to.end()));
          to = std::move(grown);
        }
        else {
          to.insert(
              to.begin(),
              std::make_move_iterator(from.end() - count),
              std::make_move_iterator(from.end()));
        }
        from.erase(from.end() - count, from.end());
        starts_[segment] -= static_cast<offset_type>(count);
        maybe_shrink(from);
      }
      else {
        // Move the front of the right neighbour to the end of this segment.
        reserve_for_insert(to, ucount);
        to.insert(
            to.end(),
            std::make_move_iterator(from.begin()),
            std::make_move_iterator(from.begin() + count));
        if(from.size() - ucount < from.capacity() / 2) {
          from = make_segment(
              std::make_move_iterator(from.begin() + count),
              std::make_move_iterator(from.end()));
        }
        else {
          from.erase(from.begin(), from.begin() + count);
        }
        starts_[neighbour] += static_cast<offset_type>(count);
      }
      return;
    }
    maybe_shrink(t_[segment]);
  }

  // Returns an iterator to the element at index in the whole container,
  // searching from the segment hint.
  iterator iterator_at(difference_type index, std::size_t hint) {
    BOOST_ASSERT(index >= 0);
    BOOST_ASSERT(index <= static_cast<difference_type>(size_));
    if(index == static_cast<difference_type>(size_)) {
      return end();
    }
    auto const segment = find_segment(index, hint);
    auto const c = t_.begin() + static_cast<difference_type>(segment);
    return iterator(
        c,
        c->begin() + (index - static_cast<difference_type>(starts_[segment])),
        this);
  }

  // Splits the segment containing idx so that idx is the first element of a
  // segment and returns the index of that segment. Never creates empty
  // segments: if idx already starts a segment nothing is split, and if idx is
  // the end of the table the returned index is t_.size().
  std::size_t split_before(Index idx) {
    auto& segment = t_[idx.segment];
    if(idx.index == 0) {
      return idx.segment;
    }
    if(idx.index == segment.size()) {
      return idx.segment + 1;
    }
    // Move the smaller part to a new allocation. The larger part keeps the
    // existing one, which it fills at least half of.
    auto const split = segment.begin() + static_cast<difference_type>(idx.index);
    std::vector<T> head;
    std::vector<T> tail;
    if(idx.index < segment.size() - idx.index) {
      head = make_segment(
          std::make_move_iterator(segment.begin()),
          std::make_move_iterator(split));
      if(segment.size() - idx.index < segment.capacity() / 2) {
        tail = make_segment(
            std::make_move_iterator(split),
            std::make_move_iterator(segment.end()));
      }
      else {
        segment.erase(segment.begin(), split);
        tail = std::move(segment);
      }
    }
    else {
      tail = make_segment(
          std::make_move_iterator(split),
          std::make_move_iterator(segment.end()));
      segment.erase(split, segment.end());
      head = std::move(segment);
    }
    t_[idx.segment] = std::move(head);
    starts_.insert(
        starts_.begin() + static_cast<difference_type>(idx.segment + 1),
        static_cast<offset_type>(starts_[idx.segment] + idx.index));
    t_.insert(
        t_.begin() + static_cast<difference_type>(idx.segment + 1),
        std::move(tail));
    return idx.segment + 1;
  }

  // Merges from into its neighbour to, which keeps its start.
  void move_contents_from_and_erase(
      typename std::vector<std::vector<T>>::iterator from,
      typename std::vector<std::vector<T>>::iterator to) {
    auto const combined = to->size() + from->size();
    if(combined <= to->capacity() && combined >= to->capacity() / 2) {
      // There's room, so only from needs copying. Erasing a range can leave
      // to holding much less than its allocation, so this is only done if
      // the result fills at least half of it.
      to->insert(
          to->end(),
          std::make_move_iterator(from->begin()),
          std::make_move_iterator(from->end()));
    }
    else {
      std::vector<T> merged;
      merged.reserve(capacity_for(combined));
      merged.insert(
          merged.end(),
          std::make_move_iterator(to->begin()),
          std::make_move_iterator(to->end()));
      merged.insert(
          merged.end(),
          std::make_move_iterator(from->begin()),
          std::make_move_iterator(from->end()));
      *to = std::move(merged);
    }
    starts_.erase(starts_.begin() + std::distance(t_.begin(), from));
    t_.erase(from);
  }

  iterator to_mutable(const_iterator it) {
    auto segment = t_.begin() + std::distance(t_.cbegin(), it.c_);
    return iterator(
        segment,
        segment->begin() + std::distance(segment->cbegin(), it.i_),
        this);
  }

  Index iterator_to_index(const_iterator it) const {
    Index idx;
    idx.segment = static_cast<std::size_t>(std::distance(t_.cbegin(), it.c_));
    idx.index = static_cast<std::size_t>(std::distance(it.c_->cbegin(), it.i_));
    return idx;
  }

  std::vector<std::vector<T>> t_;
  // starts_[s] is the index of the first element of segment s.
  std::vector<offset_type> starts_;
  std::size_t size_;
};

} // namespace daily
#endif // DAILY_CONTAINER_SEGMENTEDSEQUENCE_HPP
