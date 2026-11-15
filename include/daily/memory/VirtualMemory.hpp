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
#ifndef DAILY_MEMORY_VIRTUALMEMORY_HPP
#define DAILY_MEMORY_VIRTUALMEMORY_HPP
#pragma once

#include <boost/core/noncopyable.hpp>
#include <cstddef>

#include "daily/memory/AllocationCategory.hpp"

namespace daily {

// Part of a VirtualMemory reservation. A region doesn't own its addresses:
// it's a view that can commit and decommit pages within them, and is only
// usable while the reservation it came from is alive. Its addresses don't
// change when the reservation is moved.
//
// Committed pages are charged to the reservation's AllocationCategory. The
// accounting assumes pages are committed and decommitted once each:
// committing pages that are already committed charges them twice, and pages
// left committed when the reservation is destroyed are never uncharged.
class VirtualRegion {
 public:
  VirtualRegion() = default;

  std::byte* data() const {
    return data_;
  }

  std::size_t size() const {
    return size_;
  }

  // Makes [offset, offset + bytes) readable and writable, counting it
  // against the system's commit limit. Physical pages are only used once
  // touched. Committing pages that are already committed keeps their
  // contents. Both arguments must be multiples of
  // VirtualMemory::page_size(). Throws std::bad_alloc if the system can't
  // commit the pages.
  void commit(std::size_t offset, std::size_t bytes) const;

  // Returns [offset, offset + bytes) to the system, discarding its contents.
  // The addresses stay reserved, and read as zero if committed again. Both
  // arguments must be multiples of VirtualMemory::page_size().
  void decommit(std::size_t offset, std::size_t bytes) const;

 private:
  friend class VirtualMemory;
  VirtualRegion(std::byte* data, std::size_t size, AllocationCategory category)
      : data_(data)
      , size_(size)
      , category_(category) {
  }

  std::byte* data_ = nullptr;
  std::size_t size_ = 0;
  AllocationCategory category_ = 0;
};

// A range of reserved addresses. Reserving takes address space only: no
// memory is used, and nothing counts against the commit limit, until pages
// are committed through a VirtualRegion. The addresses are contiguous and
// fixed for the life of the reservation, so regions at chosen offsets stay
// exactly that far apart.
class VirtualMemory : boost::noncopyable {
 public:
  // The size of a page, which commits and region offsets are multiples of.
  static std::size_t page_size();

  // Rounds bytes up to a multiple of page_size().
  static std::size_t round_to_page(std::size_t bytes);

  VirtualMemory() = default;

  // Reserves at least bytes, rounded up to the system's reservation
  // granularity, charging pages committed in it to the calling thread's
  // current AllocationCategory. Throws std::bad_alloc if the address space
  // isn't available.
  explicit VirtualMemory(std::size_t bytes);

  // As above, but charging committed pages to category.
  VirtualMemory(std::size_t bytes, AllocationCategory category);

  VirtualMemory(VirtualMemory&& other) noexcept;
  VirtualMemory& operator=(VirtualMemory&& other) noexcept;

  // Releases the addresses, along with any pages still committed.
  ~VirtualMemory();

  std::byte* data() const {
    return data_;
  }

  std::size_t size() const {
    return size_;
  }

  // The bytes at [offset, offset + bytes). Both must be multiples of
  // page_size() and lie within the reservation.
  VirtualRegion region(std::size_t offset, std::size_t bytes) const;

  VirtualRegion region() const {
    return region(0, size_);
  }

 private:
  void release();

  std::byte* data_ = nullptr;
  std::size_t size_ = 0;
  AllocationCategory category_ = 0;
};

} // namespace daily

#endif // DAILY_MEMORY_VIRTUALMEMORY_HPP
