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
#ifndef DAILY_CONTAINER_TIEREDSEQUENCE_HPP
#define DAILY_CONTAINER_TIEREDSEQUENCE_HPP
#pragma once

#include "daily/detail/Prefetch.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace daily {

// A tiered vector of fixed size segments, each a circular buffer. Every
// segment other than the last is full, so the segment of an index is
// index / SegmentSize and no table of segment starts is needed. Inserting or
// erasing moves the elements within one segment and then one element across
// each later segment boundary, which for a circular buffer is a single swap
// and a change to the head.
//
// An experiment: only the operations that the ordinal container benchmarks
// need, and no exception safety beyond what they need.
template <typename T, std::size_t SegmentSize = 1024>
class TieredSequence {
  static_assert(std::has_single_bit(SegmentSize));
  static constexpr std::size_t K = SegmentSize;
  static constexpr std::size_t M = K - 1;
  static constexpr unsigned S = static_cast<unsigned>(std::countr_zero(K));

  struct Segment {
    T* data;
    std::uint32_t head;
    // How far data is into its allocation, see make_segment.
    std::uint32_t offset;
  };

 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = T const&;

  template <bool Const>
  class Iter {
    using Owner = std::conditional_t<Const, TieredSequence const, TieredSequence>;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<Const, T const*, T*>;
    using reference = std::conditional_t<Const, T const&, T&>;

    Iter() = default;
    Iter(Owner* owner, difference_type i)
        : owner_(owner)
        , i_(i) {
    }

    template <bool Other>
      requires(Const && !Other)
    Iter(Iter<Other> const& other)
        : owner_(other.owner_)
        , i_(other.i_)
        , cur_(other.cur_)
        , left_(other.left_) {
    }

    // The iterator caches a pointer to its element and how many more elements
    // follow it contiguously, so that stepping along a run of a segment is a
    // pointer increment. Like a std::vector iterator it is invalidated by
    // changes to the container.
    reference operator*() const {
      if(left_ == 0) [[unlikely]] {
        seek();
      }
      return *cur_;
    }
    pointer operator->() const {
      return &**this;
    }
    reference operator[](difference_type n) const {
      return (*owner_)[static_cast<size_type>(i_ + n)];
    }

    Iter& operator++() {
      ++i_;
      if(left_ != 0) {
        ++cur_;
        --left_;
      }
      return *this;
    }
    Iter operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }
    Iter& operator--() {
      --i_;
      left_ = 0;
      return *this;
    }
    Iter operator--(int) {
      auto copy = *this;
      --*this;
      return copy;
    }
    Iter& operator+=(difference_type n) {
      i_ += n;
      if(n >= 0 && static_cast<size_type>(n) < left_) {
        cur_ += n;
        left_ -= static_cast<size_type>(n);
      }
      else {
        left_ = 0;
      }
      return *this;
    }
    Iter& operator-=(difference_type n) {
      i_ -= n;
      left_ = 0;
      return *this;
    }
    friend Iter operator+(Iter it, difference_type n) {
      return it += n;
    }
    friend Iter operator+(difference_type n, Iter it) {
      return it += n;
    }
    friend Iter operator-(Iter it, difference_type n) {
      return it -= n;
    }
    friend difference_type operator-(Iter const& a, Iter const& b) {
      return a.i_ - b.i_;
    }
    friend bool operator==(Iter const& a, Iter const& b) {
      return a.i_ == b.i_;
    }
    friend auto operator<=>(Iter const& a, Iter const& b) {
      return a.i_ <=> b.i_;
    }

    difference_type index() const {
      return i_;
    }

   private:
    template <bool>
    friend class Iter;
    // Finds the element at i_ and the length of the run it starts.
    void seek() const {
      auto const v = static_cast<size_type>(i_) + owner_->first_;
      auto const& seg = owner_->segs_[v >> S];
      auto const offset = v & M;
      auto const phys = (seg.head + offset) & M;
      cur_ = seg.data + phys;
      left_ = std::min(K - phys, K - offset);
    }

    Owner* owner_ = nullptr;
    difference_type i_ = 0;
    mutable pointer cur_ = nullptr;
    mutable size_type left_ = 0;
  };

  using iterator = Iter<false>;
  using const_iterator = Iter<true>;

  TieredSequence() = default;
  TieredSequence(TieredSequence const& other) {
    assign(other.begin(), other.end());
  }
  TieredSequence(TieredSequence&& other) noexcept
      : buf_(std::move(other.buf_))
      , sb_(other.sb_)
      , nseg_(other.nseg_)
      , first_(other.first_)
      , size_(other.size_) {
    sync();
    other.buf_.clear();
    other.reset_empty();
  }
  TieredSequence& operator=(TieredSequence const& other) {
    if(this != &other) {
      assign(other.begin(), other.end());
    }
    return *this;
  }
  TieredSequence& operator=(TieredSequence&& other) noexcept {
    if(this != &other) {
      clear();
      buf_ = std::move(other.buf_);
      sb_ = other.sb_;
      nseg_ = other.nseg_;
      first_ = other.first_;
      size_ = other.size_;
      sync();
      other.buf_.clear();
      other.reset_empty();
    }
    return *this;
  }
  ~TieredSequence() {
    clear();
  }

  void clear() {
    for(auto& seg : buf_) {
      free_segment(seg);
    }
    buf_.clear();
    reset_empty();
  }

  size_type size() const {
    return size_;
  }
  bool empty() const {
    return size_ == 0;
  }

  reference operator[](size_type i) {
    auto const v = i + first_;
    auto const& seg = segs_[v >> S];
    return seg.data[(seg.head + (v & M)) & M];
  }
  const_reference operator[](size_type i) const {
    auto const v = i + first_;
    auto const& seg = segs_[v >> S];
    return seg.data[(seg.head + (v & M)) & M];
  }

  iterator begin() {
    return iterator(this, 0);
  }
  iterator end() {
    return iterator(this, static_cast<difference_type>(size_));
  }
  const_iterator begin() const {
    return const_iterator(this, 0);
  }
  const_iterator end() const {
    return const_iterator(this, static_cast<difference_type>(size_));
  }
  const_iterator cbegin() const {
    return begin();
  }
  const_iterator cend() const {
    return end();
  }

  template <std::forward_iterator It>
  void assign(It first, It last) {
    clear();
    auto remaining = static_cast<size_type>(std::distance(first, last));
    size_ = remaining;
    buf_.reserve((remaining + M) >> S);
    while(remaining != 0) {
      auto const n = std::min(remaining, K);
      auto seg = make_segment();
      for(size_type i = 0; i < n; ++i, ++first) {
        seg.data[i] = *first;
      }
      buf_.push_back(seg);
      remaining -= n;
    }
    nseg_ = buf_.size();
    sync();
  }

  void push_back(T value) {
    auto const length = size_ + first_;
    if((length & M) == 0) {
      append_segment(make_segment());
    }
    auto& seg = segs_[nseg_ - 1];
    seg.data[(seg.head + (length & M)) & M] = std::move(value);
    ++size_;
  }

  void pop_back() {
    --size_;
    if(size_ == 0) {
      clear();
    }
    else if(((size_ + first_) & M) == 0) {
      free_segment(buf_.back());
      buf_.pop_back();
      --nseg_;
    }
  }

  iterator insert(const_iterator pos, T value) {
    auto const p = static_cast<size_type>(pos.index());
    insert_one(p, std::move(value));
    return iterator(this, static_cast<difference_type>(p));
  }

  template <std::input_iterator It>
  iterator insert(const_iterator pos, It first, It last) {
    auto const p = static_cast<size_type>(pos.index());
    if(p == size_) {
      for(; first != last; ++first) {
        push_back(*first);
      }
    }
    else {
      std::vector<T> items(first, last);
      insert_range(p, items);
    }
    return begin() + static_cast<difference_type>(p);
  }

  // Searches like std::lower_bound and std::upper_bound, but while each probe
  // is compared the element for both possible next probes is prefetched,
  // which overlaps the cache misses of successive probes in a large
  // container.
  template <typename Compare>
  const_iterator lower_bound(
      const_iterator first,
      const_iterator last,
      T const& value,
      Compare compare) const {
    return partition_point(first, last, [&](T const& x) {
      return compare(x, value);
    });
  }

  template <typename Compare>
  const_iterator upper_bound(
      const_iterator first,
      const_iterator last,
      T const& value,
      Compare compare) const {
    return partition_point(first, last, [&](T const& x) {
      return !compare(value, x);
    });
  }

  template <typename Compare>
  iterator lower_bound(iterator first, iterator last, T const& value, Compare compare) {
    return iterator(
        this,
        lower_bound(const_iterator(first), const_iterator(last), value, compare).index());
  }

  template <typename Compare>
  iterator upper_bound(iterator first, iterator last, T const& value, Compare compare) {
    return iterator(
        this,
        upper_bound(const_iterator(first), const_iterator(last), value, compare).index());
  }

  // Like std::rotate: moves [middle, last) before [first, middle). The
  // smaller part is copied out, erased and inserted at its new position, so
  // moving a small block a long way costs a shift of the elements in between
  // rather than a swap of each of them.
  void rotate(const_iterator first, const_iterator middle, const_iterator last) {
    auto const a = static_cast<size_type>(first.index());
    auto const b = static_cast<size_type>(middle.index());
    auto const c = static_cast<size_type>(last.index());
    if(a == b || b == c) {
      return;
    }
    if(b - a <= c - b) {
      std::vector<T> moved(
          begin() + static_cast<difference_type>(a),
          begin() + static_cast<difference_type>(b));
      erase_range(a, b);
      insert_items(c - (b - a), moved);
    }
    else {
      std::vector<T> moved(
          begin() + static_cast<difference_type>(b),
          begin() + static_cast<difference_type>(c));
      erase_range(b, c);
      insert_items(a, moved);
    }
  }

  iterator erase(const_iterator pos) {
    auto const p = static_cast<size_type>(pos.index());
    erase_one(p);
    return iterator(this, static_cast<difference_type>(p));
  }

  iterator erase(const_iterator first, const_iterator last) {
    auto const f = static_cast<size_type>(first.index());
    auto const l = static_cast<size_type>(last.index());
    if(l != f) {
      erase_range(f, l);
    }
    return begin() + static_cast<difference_type>(f);
  }

 private:
  // The circular buffer slot of logical element j of a segment.
  static T& slot(Segment& seg, size_type j) {
    return seg.data[(seg.head + j) & M];
  }

  // Moves the len elements from logical index from of a segment up one
  // place, and shift_down moves them down one. Done with memmove, in up to
  // three pieces where the range wraps around the end of the buffer.
  void shift_up(Segment& seg, size_type from, size_type len) {
    if(len == 0) {
      return;
    }
    if constexpr(std::is_trivially_copyable_v<T>) {
      T* d = seg.data;
      auto const a = (seg.head + from) & M;
      auto const w = K - a;
      if(len + 1 <= w) {
        std::memmove(d + a + 1, d + a, len * sizeof(T));
      }
      else {
        if(len > w) {
          std::memmove(d + 1, d, (len - w) * sizeof(T));
        }
        d[0] = d[K - 1];
        if(w > 1) {
          std::memmove(d + a + 1, d + a, (w - 1) * sizeof(T));
        }
      }
    }
    else {
      for(auto j = from + len; j > from; --j) {
        slot(seg, j) = std::move(slot(seg, j - 1));
      }
    }
  }

  void shift_down(Segment& seg, size_type from, size_type len) {
    if(len == 0) {
      return;
    }
    if constexpr(std::is_trivially_copyable_v<T>) {
      T* d = seg.data;
      auto const a = (seg.head + from) & M;
      auto const w = K - a;
      if(a >= 1 && len <= w) {
        std::memmove(d + a - 1, d + a, len * sizeof(T));
      }
      else if(a == 0) {
        d[K - 1] = d[0];
        std::memmove(d, d + 1, (len - 1) * sizeof(T));
      }
      else {
        std::memmove(d + a - 1, d + a, w * sizeof(T));
        d[K - 1] = d[0];
        if(len > w + 1) {
          std::memmove(d, d + 1, (len - w - 1) * sizeof(T));
        }
      }
    }
    else {
      for(auto j = from; j < from + len; ++j) {
        slot(seg, j - 1) = std::move(slot(seg, j));
      }
    }
  }

  // Copies n elements within the container between virtual indices, a run
  // of the buffers at a time. Forward copies move down (dst < src) and
  // backward copies move up, so overlapping ranges are fine.
  void copy_forward(size_type dst, size_type src, size_type n) {
    while(n != 0) {
      auto& d = segs_[dst >> S];
      auto& e = segs_[src >> S];
      auto const dp = (d.head + (dst & M)) & M;
      auto const sp = (e.head + (src & M)) & M;
      auto const chunk = std::min(
          {n, K - dp, K - sp, K - (dst & M), K - (src & M)});
      if constexpr(std::is_trivially_copyable_v<T>) {
        std::memmove(d.data + dp, e.data + sp, chunk * sizeof(T));
      }
      else {
        std::move(e.data + sp, e.data + sp + chunk, d.data + dp);
      }
      dst += chunk;
      src += chunk;
      n -= chunk;
    }
  }

  void copy_backward(size_type dst, size_type src, size_type n) {
    auto dst_end = dst + n;
    auto src_end = src + n;
    while(n != 0) {
      auto& d = segs_[(dst_end - 1) >> S];
      auto& e = segs_[(src_end - 1) >> S];
      auto const dp = (d.head + ((dst_end - 1) & M)) & M;
      auto const sp = (e.head + ((src_end - 1) & M)) & M;
      auto const chunk = std::min(
          {n, dp + 1, sp + 1, ((dst_end - 1) & M) + 1, ((src_end - 1) & M) + 1});
      if constexpr(std::is_trivially_copyable_v<T>) {
        std::memmove(
            d.data + dp + 1 - chunk,
            e.data + sp + 1 - chunk,
            chunk * sizeof(T));
      }
      else {
        std::move_backward(e.data + sp + 1 - chunk, e.data + sp + 1, d.data + dp + 1);
      }
      dst_end -= chunk;
      src_end -= chunk;
      n -= chunk;
    }
  }

  // Copies n elements between two different segments, from logical index
  // from of src to logical index to of dst.
  static void
  copy_between(Segment& dst, size_type to, Segment& src, size_type from, size_type n) {
    while(n != 0) {
      auto const dp = (dst.head + to) & M;
      auto const sp = (src.head + from) & M;
      auto const chunk = std::min({n, K - dp, K - sp});
      if constexpr(std::is_trivially_copyable_v<T>) {
        std::memcpy(dst.data + dp, src.data + sp, chunk * sizeof(T));
      }
      else {
        std::move(src.data + sp, src.data + sp + chunk, dst.data + dp);
      }
      to += chunk;
      from += chunk;
      n -= chunk;
    }
  }

  void write(size_type v, T* items, size_type n) {
    while(n != 0) {
      auto& d = segs_[v >> S];
      auto const dp = (d.head + (v & M)) & M;
      auto const chunk = std::min({n, K - dp, K - (v & M)});
      std::move(items, items + chunk, d.data + dp);
      v += chunk;
      items += chunk;
      n -= chunk;
    }
  }

  // Frees segments from the end until there are just enough to hold the
  // elements.
  void trim_segments() {
    if(size_ == 0) {
      clear();
      return;
    }
    auto const needed = (size_ + first_ + M) >> S;
    while(nseg_ > needed) {
      free_segment(buf_.back());
      buf_.pop_back();
      --nseg_;
    }
  }

  // Segments are added and removed in the middle of the vector of them, which
  // moves the pointers to the ones after but none of the elements.
  void insert_fresh_segments(size_type at, size_type count) {
    buf_.insert(
        buf_.begin() + static_cast<difference_type>(sb_ + at),
        count,
        Segment{nullptr, 0, 0});
    for(size_type i = 0; i < count; ++i) {
      buf_[sb_ + at + i] = make_segment();
    }
    nseg_ += count;
    sync();
  }

  void erase_segments(size_type at, size_type count) {
    auto const first = buf_.begin() + static_cast<difference_type>(sb_ + at);
    for(auto it = first; it != first + static_cast<difference_type>(count); ++it) {
      free_segment(*it);
    }
    buf_.erase(first, first + static_cast<difference_type>(count));
    nseg_ -= count;
    sync();
  }

  // Erases [f, l) by moving the elements after it down. Elements up to the
  // next segment boundary are copied, then every later segment is a
  // whole-segment shift by the number of segments erased, and a smaller
  // shift of r places, which for a circular buffer is a change of head and
  // r elements copied across each boundary.
  void erase_range(size_type f, size_type l) {
    auto const m = l - f;
    auto const va = f + first_;
    auto const vb = l + first_;
    auto const length = size_ + first_;
    auto const tail = length - vb;
    auto const to_boundary = ((va + M) & ~M) - va;
    auto const c = std::min(to_boundary, tail);
    copy_forward(va, vb, c);
    size_ -= m;
    if(c == tail) {
      trim_segments();
      return;
    }

    auto const dest = va + c;
    auto const k = dest >> S;
    auto const q = m >> S;
    auto const r = m & M;
    auto const remaining = tail - c;
    erase_segments(k, q);
    if(r == 0) {
      return;
    }
    auto const dest_segments = (remaining + M) >> S;
    auto const source_segments = nseg_ - k;
    for(size_type i = 0; i < dest_segments; ++i) {
      auto& seg = segs_[k + i];
      seg.head = (seg.head + r) & M;
      if(i + 1 < source_segments) {
        copy_between(seg, K - r, segs_[k + i + 1], 0, r);
      }
    }
    // Any later source segment has been consumed.
    while(nseg_ > k + dest_segments) {
      free_segment(buf_.back());
      buf_.pop_back();
      --nseg_;
    }
  }

  // Inserts items at p, which is before the end, by moving the elements
  // after it up in the same way erase_range moves them down.
  void insert_range(size_type p, std::vector<T>& items) {
    auto const m = items.size();
    if(m == 0) {
      return;
    }
    auto const vp = p + first_;
    auto const length = size_ + first_;
    auto const new_length = length + m;
    auto const to_boundary = ((vp + M) & ~M) - vp;
    auto const tail = length - vp;
    auto const c = std::min(to_boundary, tail);

    auto const needed = (new_length + M) >> S;
    if(c == tail) {
      // The elements to move are all in one segment.
      while(nseg_ < needed) {
        append_segment(make_segment());
      }
      copy_backward(vp + m, vp, tail);
    }
    else {
      auto const t0 = vp + c;
      auto const k = t0 >> S;
      auto const q = m >> S;
      auto const r = m & M;
      insert_fresh_segments(k, q);
      while(nseg_ < needed) {
        append_segment(make_segment());
      }
      if(r != 0) {
        // Segments after the fresh ones shift up by r, from the last so that
        // each can still take the end of the one before.
        for(auto i = nseg_; i-- > k + q;) {
          auto& seg = segs_[i];
          seg.head = (seg.head - r) & M;
          if(i > k + q) {
            copy_between(seg, 0, segs_[i - 1], K - r, r);
          }
        }
      }
      copy_backward(vp + m, vp, c);
    }
    write(vp, items.data(), m);
    size_ += m;
  }

  void insert_items(size_type p, std::vector<T>& items) {
    if(p == size_) {
      for(auto& item : items) {
        push_back(std::move(item));
      }
    }
    else {
      insert_range(p, items);
    }
  }

  template <typename Pred>
  const_iterator partition_point(const_iterator first, const_iterator last, Pred pred) const {
    auto lo = static_cast<size_type>(first.index());
    auto len = static_cast<size_type>(last.index()) - lo;
    while(len > 2) {
      auto const half = len / 2;
      // Whichever way this probe goes, the next one is the middle of the
      // remaining half.
      detail::prefetch(&(*this)[lo + half / 2]);
      detail::prefetch(&(*this)[lo + half + 1 + (len - half - 1) / 2]);
      auto const middle = lo + half;
      bool const cond = static_cast<bool>(pred((*this)[middle]));
      lo = cond ? middle + 1 : lo;
      len = cond ? len - (half + 1) : half;
    }
    if(len == 2) {
      detail::prefetch(&(*this)[lo]);
      if(pred((*this)[lo + 1])) {
        return const_iterator(this, static_cast<difference_type>(lo + 2));
      }
      len = 1;
    }
    if(len == 1 && pred((*this)[lo])) {
      ++lo;
    }
    return const_iterator(this, static_cast<difference_type>(lo));
  }

  void sync() {
    segs_ = buf_.data() + sb_;
  }

  void reset_empty() {
    buf_.clear();
    sb_ = 0;
    nseg_ = 0;
    first_ = 0;
    size_ = 0;
    segs_ = nullptr;
  }

  // Segments are a power of two in size, so the same place in every segment
  // maps to the same few sets of the cache, and the elements that moving
  // across segments touches thrash it. Starting each at a different cache
  // line avoids that: it makes shifting across segments about 10 times
  // faster.
  static constexpr size_type kLine = std::max<size_type>(64 / sizeof(T), 1);
  static constexpr size_type kColors = 8;

  Segment make_segment() {
    auto const offset = (next_color_++ % kColors) * kLine;
    return {new T[K + kColors * kLine] + offset, 0, static_cast<std::uint32_t>(offset)};
  }

  static void free_segment(Segment& seg) {
    if(seg.data != nullptr) {
      delete[](seg.data - seg.offset);
    }
  }

  void append_segment(Segment seg) {
    buf_.push_back(seg);
    ++nseg_;
    sync();
  }

  // Segments are added at the front into a gap before the first, which is
  // regrown when it runs out so that this is amortised.
  void prepend_segment() {
    if(sb_ == 0) {
      auto const gap = std::max<size_type>(nseg_ / 2, 8);
      buf_.insert(buf_.begin(), gap, Segment{nullptr, 0, 0});
      sb_ = gap;
    }
    --sb_;
    buf_[sb_] = make_segment();
    ++nseg_;
    sync();
  }

  void drop_first_segment() {
    free_segment(buf_[sb_]);
    buf_[sb_].data = nullptr;
    ++sb_;
    --nseg_;
    sync();
  }

  // Elements are at virtual indices from first_, so that the first segment
  // can be partly empty and the front can shift as well as the back.
  void insert_one(size_type p, T value) {
    if(p == size_) {
      push_back(std::move(value));
    }
    else if(p < size_ / 2) {
      insert_shifting_front(p, std::move(value));
    }
    else {
      insert_shifting_back(p, std::move(value));
    }
  }

  void insert_shifting_back(size_type p, T value) {
    auto const v = p + first_;
    auto const length = size_ + first_;
    auto const s = v >> S;
    auto const o = v & M;
    auto const last = nseg_ - 1;
    auto const last_count = length - (last << S);
    auto& seg = segs_[s];

    if(s == last && last_count < K) {
      shift_up(seg, o, last_count - o);
      slot(seg, o) = std::move(value);
      ++size_;
      return;
    }

    // The segment is full, so its last element moves to the next segment.
    T carry = std::move(slot(seg, K - 1));
    shift_up(seg, o, K - 1 - o);
    slot(seg, o) = std::move(value);

    // Each later full segment takes the carry as its new first element,
    // which is the slot of its old last element once the head moves back.
    for(auto t = s + 1; t < last; ++t) {
      auto& next = segs_[t];
      next.head = (next.head - 1) & M;
      std::swap(carry, next.data[next.head]);
    }

    bool have_carry = true;
    if(s < last) {
      auto& tail = segs_[last];
      tail.head = (tail.head - 1) & M;
      if(last_count < K) {
        tail.data[tail.head] = std::move(carry);
        have_carry = false;
      }
      else {
        std::swap(carry, tail.data[tail.head]);
      }
    }
    if(have_carry) {
      append_segment(make_segment());
      slot(segs_[nseg_ - 1], 0) = std::move(carry);
    }
    ++size_;
  }

  // Moves the elements before p down one place, using the space before
  // first_ (adding a segment for it if there is none), and puts value at p.
  void insert_shifting_front(size_type p, T value) {
    if(first_ == 0) {
      prepend_segment();
      first_ = K;
    }
    --first_;
    auto const w = p + first_;
    auto const sw = w >> S;
    auto const ow = w & M;

    T incoming{};
    auto& target = segs_[sw];
    if(sw == 0) {
      shift_down(target, first_ + 1, ow - first_);
      slot(target, ow) = std::move(value);
    }
    else {
      incoming = std::move(slot(target, 0));
      shift_down(target, 1, ow);
      slot(target, ow) = std::move(value);

      // The first element of each earlier segment is the element before the
      // one that moved into it.
      for(size_type t = 0; t + 1 < sw; ++t) {
        auto& a = segs_[t];
        auto& b = segs_[t + 1];
        a.head = (a.head + 1) & M;
        slot(a, K - 1) = std::move(b.data[b.head]);
      }
      auto& a = segs_[sw - 1];
      a.head = (a.head + 1) & M;
      slot(a, K - 1) = std::move(incoming);
    }
    ++size_;
  }

  void erase_one(size_type p) {
    if(size_ == 1) {
      clear();
    }
    else if(p < size_ / 2) {
      erase_shifting_front(p);
    }
    else {
      erase_shifting_back(p);
    }
  }

  void erase_shifting_back(size_type p) {
    auto const v = p + first_;
    auto const length = size_ + first_;
    auto const s = v >> S;
    auto const o = v & M;
    auto const last = nseg_ - 1;
    auto& seg = segs_[s];

    if(s == last) {
      auto const count = length - (last << S);
      shift_down(seg, o + 1, count - o - 1);
    }
    else {
      shift_down(seg, o + 1, K - 1 - o);
      // Each later segment gives up its first element to fill the end of the
      // one before it, and its head moves on so that the slot it left is its
      // new last.
      for(auto t = s + 1; t <= last; ++t) {
        auto& prev = segs_[t - 1];
        auto& next = segs_[t];
        slot(prev, K - 1) = std::move(next.data[next.head]);
        next.head = (next.head + 1) & M;
      }
    }
    --size_;
    if(((size_ + first_) & M) == 0) {
      free_segment(buf_.back());
      buf_.pop_back();
      --nseg_;
    }
  }

  // Moves the elements before p up one place, which leaves one more empty
  // place before them.
  void erase_shifting_front(size_type p) {
    auto const v = p + first_;
    auto const sv = v >> S;
    auto const ov = v & M;

    T carry{};
    for(size_type t = 0; t < sv; ++t) {
      auto& a = segs_[t];
      a.head = (a.head - 1) & M;
      std::swap(carry, a.data[a.head]);
    }
    auto& target = segs_[sv];
    if(sv == 0) {
      shift_up(target, first_, ov - first_);
    }
    else {
      shift_up(target, 0, ov);
      slot(target, 0) = std::move(carry);
    }
    ++first_;
    --size_;
    if(first_ == K) {
      drop_first_segment();
      first_ = 0;
    }
  }

  // Segments from sb_ on are live. The ones before are a gap.
  std::vector<Segment> buf_;
  size_type sb_ = 0;
  Segment* segs_ = nullptr;
  size_type nseg_ = 0;
  // Number of unused places at the front of the first segment.
  size_type first_ = 0;
  size_type size_ = 0;
  size_type next_color_ = 0;
};

} // namespace daily
#endif // DAILY_CONTAINER_TIEREDSEQUENCE_HPP
