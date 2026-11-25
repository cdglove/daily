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
#ifdef _WIN32
#  define NOMINMAX            1
#  define WIN32_LEAN_AND_MEAN 1
#endif

#include "daily/memory/AllocationCategory.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#  include <malloc.h>
#  include <windows.h>
#elif defined(__APPLE__)
#  include <malloc/malloc.h>
#  include <os/lock.h>
#  include <sys/mman.h>
#  include <unistd.h>
#else
#  ifdef __linux__
#    include <malloc.h>
#  endif
#  include <pthread.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace {

// Stored immediately before the pointer returned to the caller for heap
// allocations served by malloc.
struct Header {
  std::uint64_t size;
  daily::AllocationCategory category;
};

// Space reserved in front of allocations that need no more than the default
// alignment. Larger alignments reserve their alignment instead.
constexpr std::size_t kHeaderSpace = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
static_assert(kHeaderSpace >= sizeof(Header));
static_assert(kHeaderSpace >= alignof(std::max_align_t));

// Allocations whose malloc block would exceed 1MiB (and need no alignment
// beyond the OS allocation granularity) bypass malloc and map whole pages
// directly from the OS:
// - Freeing them unmaps the pages immediately instead of leaving multi-MiB
//   buffers in deferred kernel reclamation (such as XNU's vm_reclaim ring on
//   macOS) or heap free caches, while keeping smaller transient allocations
//   inside malloc to avoid page-fault and TLB-shootdown churn.
// - Their metadata lives in a small out-of-band table rather than in front of
//   the payload, so power-of-two and page-multiple buffers never spill into an
//   extra page.
constexpr std::size_t kMallocMaxBytes = std::size_t{1} << 20;

std::size_t header_space(std::size_t align) noexcept {
  return std::max(align, kHeaderSpace);
}

Header* header_of(void* p) noexcept {
  return reinterpret_cast<Header*>(static_cast<std::byte*>(p) - sizeof(Header));
}

std::size_t round_up(std::size_t bytes, std::size_t multiple) noexcept {
  return (bytes + multiple - 1) / multiple * multiple;
}

std::size_t usable_block_size(
    void* base,
    std::size_t requested,
    [[maybe_unused]] std::size_t align) noexcept {
#if defined(__APPLE__)
  std::size_t const actual = malloc_size(base);
  return actual != 0 ? actual : requested;
#elif defined(_WIN32)
  std::size_t const actual = align > kHeaderSpace ? _aligned_msize(base, align, 0)
                                                  : _msize(base);
  return actual != static_cast<std::size_t>(-1) ? actual : requested;
#elif defined(__linux__)
  std::size_t const actual = malloc_usable_size(base);
  return actual != 0 ? actual : requested;
#else
  return requested;
#endif
}

constinit std::atomic<std::size_t> g_page_size{0};
constinit std::atomic<std::size_t> g_alloc_granularity{0};

void init_system_sizes(std::size_t& page, std::size_t& granularity) noexcept {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  page = info.dwPageSize;
  granularity = info.dwAllocationGranularity;
#else
  page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
  granularity = page;
#endif
  g_page_size.store(page, std::memory_order_relaxed);
  g_alloc_granularity.store(granularity, std::memory_order_relaxed);
}

std::size_t page_size() noexcept {
  std::size_t page = g_page_size.load(std::memory_order_relaxed);
  if(page == 0) [[unlikely]] {
    std::size_t granularity = 0;
    init_system_sizes(page, granularity);
  }
  return page;
}

std::size_t alloc_granularity() noexcept {
  std::size_t granularity = g_alloc_granularity.load(std::memory_order_relaxed);
  if(granularity == 0) [[unlikely]] {
    std::size_t page = 0;
    init_system_sizes(page, granularity);
  }
  return granularity;
}

void* os_map(std::size_t bytes) noexcept {
#ifdef _WIN32
  return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
  void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  return p == MAP_FAILED ? nullptr : p;
#endif
}

void os_unmap(void* p, [[maybe_unused]] std::size_t bytes) noexcept {
#ifdef _WIN32
  VirtualFree(p, 0, MEM_RELEASE);
#else
  munmap(p, bytes);
#endif
}

class TableLock {
 public:
  TableLock() noexcept {
#if defined(__APPLE__)
    os_unfair_lock_lock(&g_lock_);
#elif defined(_WIN32)
    AcquireSRWLockExclusive(&g_lock_);
#else
    pthread_mutex_lock(&g_lock_);
#endif
  }

  ~TableLock() {
#if defined(__APPLE__)
    os_unfair_lock_unlock(&g_lock_);
#elif defined(_WIN32)
    ReleaseSRWLockExclusive(&g_lock_);
#else
    pthread_mutex_unlock(&g_lock_);
#endif
  }

  TableLock(TableLock const&) = delete;
  TableLock& operator=(TableLock const&) = delete;

 private:
#if defined(__APPLE__)
  static constinit os_unfair_lock g_lock_;
#elif defined(_WIN32)
  static constinit SRWLOCK g_lock_;
#else
  static constinit pthread_mutex_t g_lock_;
#endif
};

#if defined(__APPLE__)
constinit os_unfair_lock TableLock::g_lock_ = OS_UNFAIR_LOCK_INIT;
#elif defined(_WIN32)
constinit SRWLOCK TableLock::g_lock_ = SRWLOCK_INIT;
#else
constinit pthread_mutex_t TableLock::g_lock_ = PTHREAD_MUTEX_INITIALIZER;
#endif

// Out-of-band entry for an OS-mapped allocation. Mapped sizes are multiples of
// the OS page size (>= 4096), so the low 8 bits are always zero and pack the
// allocation category in 16 bytes per entry.
struct VirtualEntry {
  void* ptr;
  std::size_t bytes_and_category;
};

constexpr std::size_t kCategoryMask = 0xFF;

std::size_t pack_entry(std::size_t bytes, daily::AllocationCategory category) noexcept {
  return bytes | static_cast<std::size_t>(category);
}

std::size_t entry_bytes(VirtualEntry const& e) noexcept {
  return e.bytes_and_category & ~kCategoryMask;
}

daily::AllocationCategory entry_category(VirtualEntry const& e) noexcept {
  return static_cast<daily::AllocationCategory>(
      e.bytes_and_category & kCategoryMask);
}

std::size_t hash_ptr(void const* p) noexcept {
  auto x = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  return static_cast<std::size_t>(x);
}

// Open-addressed linear-probing table backed directly by os_map and never
// destroyed, so allocations before main() and frees during exit() remain safe.
struct VirtualTable {
  VirtualEntry* entries = nullptr;
  std::size_t capacity = 0;
  std::size_t count = 0;
  std::atomic<std::size_t> live_count{0};
};

constinit VirtualTable g_virtual_table{};

bool grow_virtual_table() noexcept {
  std::size_t const old_cap = g_virtual_table.capacity;
  std::size_t const min_bytes = page_size();
  std::size_t const new_cap = old_cap == 0 ? std::max<std::size_t>(
                                                 256,
                                                 min_bytes / sizeof(VirtualEntry))
                                           : old_cap * 2;
  std::size_t const old_bytes = old_cap * sizeof(VirtualEntry);
  std::size_t const new_bytes = new_cap * sizeof(VirtualEntry);
  auto* new_entries = static_cast<VirtualEntry*>(os_map(new_bytes));
  if(new_entries == nullptr) {
    return false;
  }

  std::size_t const mask = new_cap - 1;
  VirtualEntry* const old_entries = g_virtual_table.entries;
  for(std::size_t i = 0; i < old_cap; ++i) {
    if(old_entries[i].ptr != nullptr) {
      std::size_t slot = hash_ptr(old_entries[i].ptr) & mask;
      while(new_entries[slot].ptr != nullptr) {
        slot = (slot + 1) & mask;
      }
      new_entries[slot] = old_entries[i];
    }
  }

  if(old_entries != nullptr) {
    os_unmap(old_entries, old_bytes);
  }
  g_virtual_table.entries = new_entries;
  g_virtual_table.capacity = new_cap;
  daily::detail::add_heap_bytes(0, static_cast<std::int64_t>(new_bytes - old_bytes));
  return true;
}

void* allocate_virtual(std::size_t size) noexcept {
  std::size_t const bytes = round_up(size, page_size());
  void* p = os_map(bytes);
  if(p == nullptr) {
    return nullptr;
  }

  daily::AllocationCategory const category = daily::current_allocation_category();
  {
    TableLock const lock;
    if((g_virtual_table.count + 1) * 2 > g_virtual_table.capacity &&
       !grow_virtual_table()) {
      os_unmap(p, bytes);
      return nullptr;
    }
    std::size_t const mask = g_virtual_table.capacity - 1;
    std::size_t slot = hash_ptr(p) & mask;
    while(g_virtual_table.entries[slot].ptr != nullptr) {
      slot = (slot + 1) & mask;
    }
    g_virtual_table.entries[slot] = {p, pack_entry(bytes, category)};
    g_virtual_table.live_count.store(
        ++g_virtual_table.count,
        std::memory_order_release);
  }

  daily::detail::add_heap_bytes(category, static_cast<std::int64_t>(bytes));
  return p;
}

bool deallocate_virtual(void* p) noexcept {
  VirtualEntry removed{nullptr, 0};
  {
    TableLock const lock;
    if(g_virtual_table.count == 0) {
      return false;
    }
    VirtualEntry* const entries = g_virtual_table.entries;
    std::size_t const mask = g_virtual_table.capacity - 1;
    std::size_t i = hash_ptr(p) & mask;
    for(;;) {
      if(entries[i].ptr == nullptr) {
        return false;
      }
      if(entries[i].ptr == p) {
        break;
      }
      i = (i + 1) & mask;
    }
    removed = entries[i];

    // Backward-shift subsequent entries in the cluster rather than leaving
    // tombstones, so steady-state alloc/free churn never degrades probe length.
    std::size_t j = i;
    for(;;) {
      j = (j + 1) & mask;
      if(entries[j].ptr == nullptr) {
        break;
      }
      std::size_t const k = hash_ptr(entries[j].ptr) & mask;
      bool const can_stay = (i < j) ? (k > i && k <= j) : (k > i || k <= j);
      if(!can_stay) {
        entries[i] = entries[j];
        i = j;
      }
    }
    entries[i] = {nullptr, 0};
    g_virtual_table.live_count.store(
        --g_virtual_table.count,
        std::memory_order_release);
  }

  std::size_t const bytes = entry_bytes(removed);
  daily::detail::add_heap_bytes(
      entry_category(removed),
      -static_cast<std::int64_t>(bytes));
  os_unmap(p, bytes);
  return true;
}

void* allocate(std::size_t size, std::size_t align) {
  std::size_t const space = header_space(align);
  std::size_t const granularity = alloc_granularity();
  if(size > SIZE_MAX - std::max(2 * space, granularity)) {
    return nullptr;
  }

  if(space + size > kMallocMaxBytes && align <= granularity) {
    return allocate_virtual(size);
  }

  void* base = nullptr;
  std::size_t requested = space + size;
  if(align <= kHeaderSpace) {
    base = std::malloc(requested);
  }
  else {
#ifdef _WIN32
    base = _aligned_malloc(requested, align);
#else
    // aligned_alloc needs a size that's a multiple of the alignment.
    requested = round_up(requested, align);
    base = std::aligned_alloc(align, requested);
#endif
  }
  if(base == nullptr) {
    return nullptr;
  }

  std::size_t const charged = usable_block_size(base, requested, align);
  void* p = static_cast<std::byte*>(base) + space;
  Header* h = header_of(p);
  h->size = charged;
  h->category = daily::current_allocation_category();
  daily::detail::add_heap_bytes(h->category, static_cast<std::int64_t>(charged));
  return p;
}

void deallocate(void* p, std::size_t align) noexcept {
  if(p == nullptr) {
    return;
  }

  std::size_t const granularity = alloc_granularity();
  if((reinterpret_cast<std::uintptr_t>(p) & (granularity - 1)) == 0 &&
     g_virtual_table.live_count.load(std::memory_order_acquire) != 0 &&
     deallocate_virtual(p)) {
    return;
  }

  Header const* h = header_of(p);
  daily::detail::add_heap_bytes(h->category, -static_cast<std::int64_t>(h->size));
  void* base = static_cast<std::byte*>(p) - header_space(align);
#ifdef _WIN32
  if(align > kHeaderSpace) {
    _aligned_free(base);
    return;
  }
#endif
  std::free(base);
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
