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

// Replaces the global operator new and delete with versions that charge each
// allocation to the current AllocationCategory. Compiled into the executable
// that links daily::memory_new, so only executables should link it.
#include "daily/memory/AllocationCategory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#  include <malloc.h>
#endif

namespace {

// Stored immediately before the pointer returned to the caller.
struct Header {
  std::uint64_t size;
  daily::AllocationCategory category;
};

// Space reserved in front of allocations that need no more than the default
// alignment. Larger alignments reserve their alignment instead.
constexpr std::size_t kHeaderSpace = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
static_assert(kHeaderSpace >= sizeof(Header));
static_assert(kHeaderSpace >= alignof(std::max_align_t));

std::size_t header_space(std::size_t align) {
  return std::max(align, kHeaderSpace);
}

Header* header_of(void* p) {
  return reinterpret_cast<Header*>(static_cast<std::byte*>(p) - sizeof(Header));
}

void* allocate(std::size_t size, std::size_t align) {
  std::size_t const space = header_space(align);
  if(size > SIZE_MAX - 2 * space) {
    return nullptr;
  }

  void* base = nullptr;
  if(align <= kHeaderSpace) {
    base = std::malloc(space + size);
  }
  else {
#ifdef _WIN32
    base = _aligned_malloc(space + size, align);
#else
    // aligned_alloc needs a size that's a multiple of the alignment.
    std::size_t const total = (space + size + align - 1) / align * align;
    base = std::aligned_alloc(align, total);
#endif
  }
  if(base == nullptr) {
    return nullptr;
  }

  void* p = static_cast<std::byte*>(base) + space;
  Header* h = header_of(p);
  h->size = size;
  h->category = daily::current_allocation_category();
  daily::detail::add_heap_bytes(h->category, static_cast<std::int64_t>(size));
  return p;
}

void deallocate(void* p, std::size_t align) noexcept {
  if(p == nullptr) {
    return;
  }

  Header const* h = header_of(p);
  daily::detail::add_heap_bytes(h->category, -static_cast<std::int64_t>(h->size));
  void* base = static_cast<std::byte*>(p) - header_space(align);
  if(align <= kHeaderSpace) {
    std::free(base);
  }
  else {
#ifdef _WIN32
    _aligned_free(base);
#else
    std::free(base);
#endif
  }
}

// Calls the new handler until the allocation succeeds, as the standard's
// operator new does.
void* allocate_or_throw(std::size_t size, std::size_t align) {
  for(;;) {
    if(void* p = allocate(size, align)) {
      return p;
    }
    std::new_handler handler = std::get_new_handler();
    if(handler == nullptr) {
      throw std::bad_alloc();
    }
    handler();
  }
}

void* allocate_or_null(std::size_t size, std::size_t align) noexcept {
  try {
    return allocate_or_throw(size, align);
  }
  catch(...) {
    return nullptr;
  }
}

std::size_t to_size(std::align_val_t align) {
  return static_cast<std::size_t>(align);
}

} // namespace

void* operator new(std::size_t size) {
  return allocate_or_throw(size, kHeaderSpace);
}

void* operator new[](std::size_t size) {
  return allocate_or_throw(size, kHeaderSpace);
}

void* operator new(std::size_t size, std::align_val_t align) {
  return allocate_or_throw(size, to_size(align));
}

void* operator new[](std::size_t size, std::align_val_t align) {
  return allocate_or_throw(size, to_size(align));
}

void* operator new(std::size_t size, std::nothrow_t const&) noexcept {
  return allocate_or_null(size, kHeaderSpace);
}

void* operator new[](std::size_t size, std::nothrow_t const&) noexcept {
  return allocate_or_null(size, kHeaderSpace);
}

void* operator new(std::size_t size, std::align_val_t align, std::nothrow_t const&) noexcept {
  return allocate_or_null(size, to_size(align));
}

void* operator new[](std::size_t size, std::align_val_t align, std::nothrow_t const&) noexcept {
  return allocate_or_null(size, to_size(align));
}

void operator delete(void* p) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete[](void* p) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete(void* p, std::size_t /* size */) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete[](void* p, std::size_t /* size */) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete(void* p, std::align_val_t align) noexcept {
  deallocate(p, to_size(align));
}

void operator delete[](void* p, std::align_val_t align) noexcept {
  deallocate(p, to_size(align));
}

void operator delete(void* p, std::size_t /* size */, std::align_val_t align) noexcept {
  deallocate(p, to_size(align));
}

void operator delete[](void* p, std::size_t /* size */, std::align_val_t align) noexcept {
  deallocate(p, to_size(align));
}

void operator delete(void* p, std::nothrow_t const&) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete[](void* p, std::nothrow_t const&) noexcept {
  deallocate(p, kHeaderSpace);
}

void operator delete(void* p, std::align_val_t align, std::nothrow_t const&) noexcept {
  deallocate(p, to_size(align));
}

void operator delete[](void* p, std::align_val_t align, std::nothrow_t const&) noexcept {
  deallocate(p, to_size(align));
}
