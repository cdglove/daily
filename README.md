# daily

General purpose C++20 utilities. Each component is its own CMake target, so a
project only pulls in what it uses.

| Target | What it is |
|---|---|
| `daily::algorithm` | `parallel_sort`, which splits a sort into tasks for an executor |
| `daily::container` | `SegmentedSequence` and `TieredSequence`, sequences where inserting or erasing in the middle only moves the elements of one segment, and `VirtualVector`, a contiguous vector whose elements never move as it grows |
| `daily::memory` | `VirtualMemory`, reserved address space committed as it's used, and allocation categories, which account for memory by the part of a program that uses it |
| `daily::memory_new` | Replaces the global `operator new` and `delete` so heap memory is charged to allocation categories. For executables only |
| `daily::synchronisation` | `TicketGate`, which makes a reader-writer lock grant its locks in the order they're asked for, without making readers queue behind each other |
| `daily::utility` | `compact_optional`, an optional the size of its type, which uses one value of the type to mean empty, and Boost.Serialization support for it |
| `daily::future` | A future and promise with continuations, which can run where the result is set, where it's asked for, or on an executor |
| `daily::fast_iostream` | `FastOStream` and `FastIStream`, which read and write numbers without the locale or formatting flags |
| `daily::trace` | Verbosity based logging |
| `daily::program_options` | `key=value` command line arguments, read into registered variables, for when Boost.ProgramOptions is more than needed |

## Using it

With FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
  daily
  GIT_REPOSITORY https://github.com/cdglove/daily.git
  GIT_TAG <commit>)
FetchContent_MakeAvailable(daily)

target_link_libraries(my_target PRIVATE daily::container daily::memory)
```

daily fetches Boost with `FetchContent` unless the parent project has already
declared `Boost` itself. Its tests and benchmarks aren't built unless
`DAILY_BUILD_TESTING` or `DAILY_BUILD_BENCHMARKS` is set.

## Building it on its own

You need CMake 3.25 or later and a C++20 compiler. CMake downloads Boost and
Google Benchmark.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

`DAILY_MEMORY_CATEGORIES=OFF` turns allocation categories into no-ops.

## Licence

daily is licensed under the [Apache License, Version 2.0](LICENSE).
