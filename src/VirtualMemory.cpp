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
#ifdef _WIN32
#  define NOMINMAX            1
#  define WIN32_LEAN_AND_MEAN 1
#endif

#include "daily/memory/VirtualMemory.hpp"

#include <boost/assert.hpp>
#include <cstdint>
#include <new>
#include <utility>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace daily {
namespace {

#ifdef _WIN32

SYSTEM_INFO const& system_info() {
  static SYSTEM_INFO const info = [] {
    SYSTEM_INFO i;
    GetSystemInfo(&i);
    return i;
  }();
  return info;
}

std::size_t granularity() {
  return system_info().dwAllocationGranularity;
}

std::byte* reserve(std::size_t bytes) {
  return static_cast<std::byte*>(
      VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS));
}

void release(std::byte* data, std::size_t /* bytes */) {
  VirtualFree(data, 0, MEM_RELEASE);
}

bool commit(std::byte* data, std::size_t bytes) {
  return VirtualAlloc(data, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void decommit(std::byte* data, std::size_t bytes) {
  VirtualFree(data, bytes, MEM_DECOMMIT);
}

#else

std::size_t granularity() {
  return VirtualMemory::page_size();
}

// MAP_NORESERVE keeps Linux from counting the reservation against the
// commit limit. Making pages writable later counts them.
constexpr int kReserveFlags = MAP_PRIVATE | MAP_ANON | MAP_NORESERVE;

std::byte* reserve(std::size_t bytes) {
  void* p = mmap(nullptr, bytes, PROT_NONE, kReserveFlags, -1, 0);
  return p == MAP_FAILED ? nullptr : static_cast<std::byte*>(p);
}

void release(std::byte* data, std::size_t bytes) {
  munmap(data, bytes);
}

bool commit(std::byte* data, std::size_t bytes) {
  return mprotect(data, bytes, PROT_READ | PROT_WRITE) == 0;
}

// Mapping fresh inaccessible pages over the range frees the old ones on both
// Linux and macOS, whose madvise flags for this differ.
void decommit(std::byte* data, std::size_t bytes) {
  [[maybe_unused]] void* p =
      mmap(data, bytes, PROT_NONE, kReserveFlags | MAP_FIXED, -1, 0);
  BOOST_ASSERT(p == data);
}

#endif

std::size_t round_up(std::size_t bytes, std::size_t multiple) {
  return (bytes + multiple - 1) / multiple * multiple;
}

} // namespace

void VirtualRegion::commit(std::size_t offset, std::size_t bytes) const {
  BOOST_ASSERT(offset % VirtualMemory::page_size() == 0);
  BOOST_ASSERT(bytes % VirtualMemory::page_size() == 0);
  BOOST_ASSERT(offset <= size_ && bytes <= size_ - offset);
  if(bytes == 0) {
    return;
  }
  if(!daily::commit(data_ + offset, bytes)) {
    throw std::bad_alloc();
  }
  detail::add_committed_bytes(category_, static_cast<std::int64_t>(bytes));
}

void VirtualRegion::decommit(std::size_t offset, std::size_t bytes) const {
  BOOST_ASSERT(offset % VirtualMemory::page_size() == 0);
  BOOST_ASSERT(bytes % VirtualMemory::page_size() == 0);
  BOOST_ASSERT(offset <= size_ && bytes <= size_ - offset);
  if(bytes != 0) {
    daily::decommit(data_ + offset, bytes);
    detail::add_committed_bytes(category_, -static_cast<std::int64_t>(bytes));
  }
}

std::size_t VirtualMemory::page_size() {
#ifdef _WIN32
  static std::size_t const size = system_info().dwPageSize;
#else
  static std::size_t const size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
  return size;
}

std::size_t VirtualMemory::round_to_page(std::size_t bytes) {
  return round_up(bytes, page_size());
}

VirtualMemory::VirtualMemory(std::size_t bytes)
    : VirtualMemory(bytes, current_allocation_category()) {
}

VirtualMemory::VirtualMemory(std::size_t bytes, AllocationCategory category)
    : category_(category) {
  if(bytes == 0) {
    return;
  }
  std::size_t const size = round_up(bytes, granularity());
  data_ = reserve(size);
  if(data_ == nullptr) {
    throw std::bad_alloc();
  }
  size_ = size;
}

VirtualMemory::VirtualMemory(VirtualMemory&& other) noexcept
    : data_(std::exchange(other.data_, nullptr))
    , size_(std::exchange(other.size_, 0))
    , category_(std::exchange(other.category_, 0)) {
}

VirtualMemory& VirtualMemory::operator=(VirtualMemory&& other) noexcept {
  if(this != &other) {
    release();
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    category_ = std::exchange(other.category_, 0);
  }
  return *this;
}

VirtualMemory::~VirtualMemory() {
  release();
}

VirtualRegion VirtualMemory::region(std::size_t offset, std::size_t bytes) const {
  BOOST_ASSERT(offset % page_size() == 0);
  BOOST_ASSERT(bytes % page_size() == 0);
  BOOST_ASSERT(offset <= size_ && bytes <= size_ - offset);
  return VirtualRegion(data_ + offset, bytes, category_);
}

void VirtualMemory::release() {
  if(data_ != nullptr) {
    daily::release(data_, size_);
    data_ = nullptr;
    size_ = 0;
  }
}

} // namespace daily
