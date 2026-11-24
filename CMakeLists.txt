# Copyright (c) 2026 Christopher Glover. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# General purpose utilities. Each component is its own target, so a project
# only pulls in what it uses.
#
# Built on its own, this downloads Boost and Google Benchmark, and builds its
# tests and benchmarks. Included in another project, such as with
# FetchContent, it finds Boost through find_package, from the project, and
# leaves the tests and benchmarks out unless DAILY_BUILD_TESTING or
# DAILY_BUILD_BENCHMARKS is set.
cmake_minimum_required(VERSION 3.25)

project(daily LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  include(FetchContent)
  FetchContent_Declare(
    Boost
    SYSTEM
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
    URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
    URL_HASH MD5=542f895860ceb7a99c9aa4d02f6073c8
    DOWNLOAD_EXTRACT_TIMESTAMP True)
  FetchContent_MakeAvailable(Boost)

  set(BENCHMARK_ENABLE_TESTING
      OFF
      CACHE BOOL "" FORCE)
  FetchContent_Declare(
    benchmark
    SYSTEM
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
    URL https://github.com/google/benchmark/archive/refs/tags/v1.10.0.tar.gz
    URL_HASH MD5=046d7d6b9a11db09693af9ea2a8c7a82
    USES_TERMINAL_DOWNLOAD TRUE)
  FetchContent_MakeAvailable(benchmark)
endif()

# Algorithms
add_library(daily_algorithm INTERFACE)
add_library(daily::algorithm ALIAS daily_algorithm)

target_sources(daily_algorithm
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/daily/algorithm/ParallelSort.hpp
)

target_compile_features(daily_algorithm INTERFACE cxx_std_20)

find_package(Boost REQUIRED)

# Memory
option(DAILY_MEMORY_CATEGORIES "Track allocations by category" ON)

add_library(daily_memory STATIC src/AllocationCategory.cpp src/VirtualMemory.cpp)
add_library(daily::memory ALIAS daily_memory)

target_sources(daily_memory
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/daily/memory/AllocationCategory.hpp
      include/daily/memory/VirtualMemory.hpp
)

target_compile_features(daily_memory PUBLIC cxx_std_20)

target_compile_definitions(
  daily_memory
  PUBLIC DAILY_MEMORY_CATEGORIES=$<BOOL:${DAILY_MEMORY_CATEGORIES}>)

target_link_libraries(daily_memory PUBLIC Boost::assert Boost::core)

# Replaces the global operator new and delete so heap allocations are charged
# to allocation categories. The source compiles into whatever links this, so
# only executables should link it. Empty when DAILY_MEMORY_CATEGORIES is off.
add_library(daily_memory_new INTERFACE)
add_library(daily::memory_new ALIAS daily_memory_new)

if(DAILY_MEMORY_CATEGORIES)
  target_sources(daily_memory_new
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/AllocationCategoryNew.cpp)
endif()

target_link_libraries(daily_memory_new INTERFACE daily::memory)

# Container
add_library(daily_container INTERFACE)
add_library(daily::container ALIAS daily_container)

target_sources(daily_container
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/daily/container/SegmentedSequence.hpp
      include/daily/container/TieredSequence.hpp
      include/daily/container/VirtualVector.hpp
      include/daily/detail/Prefetch.hpp
)

target_compile_features(daily_container INTERFACE cxx_std_20)

target_link_libraries(daily_container
  INTERFACE
    Boost::headers
    Boost::stl_interfaces
    daily::memory
)

# Utility
add_library(daily_utility INTERFACE)
add_library(daily::utility ALIAS daily_utility)

target_sources(daily_utility
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/daily/utility/CompactOptional.hpp
      include/daily/CompactOptional.hpp
      include/daily/support/BoostSerialization.hpp
)

target_compile_features(daily_utility INTERFACE cxx_std_20)

# Fast iostream
add_library(daily_fast_iostream STATIC src/FastLocale.cpp)
add_library(daily::fast_iostream ALIAS daily_fast_iostream)

target_sources(daily_fast_iostream
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/daily/fast_iostream/FastIStream.hpp
      include/daily/fast_iostream/FastLocale.hpp
      include/daily/fast_iostream/FastOStream.hpp
      include/daily/fast_iostream/detail/Format.hpp
)

target_compile_features(daily_fast_iostream PUBLIC cxx_std_20)

# Future
add_library(daily_future INTERFACE)
add_library(daily::future ALIAS daily_future)

target_sources(daily_future
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/daily/future/DefaultAllocator.hpp
      include/daily/future/Future.hpp
      include/daily/future/UseFuture.hpp
)

target_compile_features(daily_future INTERFACE cxx_std_20)

target_link_libraries(daily_future INTERFACE Boost::asio Boost::optional Boost::throw_exception)

# Program options
add_library(daily_program_options INTERFACE)
add_library(daily::program_options ALIAS daily_program_options)

target_sources(daily_program_options
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/daily/program_options/ProgramOptions.hpp
)

target_compile_features(daily_program_options INTERFACE cxx_std_20)

# Trace
add_library(daily_trace STATIC src/Log.cpp)
add_library(daily::trace ALIAS daily_trace)

target_sources(daily_trace
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/daily/trace/Log.hpp
)

target_compile_features(daily_trace PUBLIC cxx_std_20)

option(DAILY_BUILD_TESTING "Build daily tests" ${PROJECT_IS_TOP_LEVEL})
if(DAILY_BUILD_TESTING)
  include(CTest)
  find_package(Boost REQUIRED unit_test_framework serialization)

  add_executable(
    test-daily
    test/test-daily.cpp
    test/test-daily-algorithm-ParallelSort.cpp
    test/test-daily-container-SegmentedSequence.cpp
    test/test-daily-container-VirtualVector.cpp
    test/test-daily-fast_iostream-FastStream.cpp
    test/test-daily-future-Future.cpp
    test/test-daily-future-PackagedTask.cpp
    test/test-daily-future-UseFuture.cpp
    test/test-daily-memory-AllocationCategory.cpp
    test/test-daily-memory-VirtualMemory.cpp
    test/test-daily-program_options-ProgramOptions.cpp
    test/test-daily-trace-Log.cpp
    test/test-daily-utility-CompactOptional.cpp
    test/test-daily-support-BoostSerialization.cpp)
  target_link_libraries(
    test-daily
    PRIVATE daily::algorithm
            daily::container
            daily::fast_iostream
            daily::future
            daily::memory
            daily::program_options
            daily::trace
            daily::utility
            Boost::serialization
            Boost::unit_test_framework)
  add_test(NAME test-daily COMMAND test-daily)

  # Separate so the replaced operator new and delete don't affect the other
  # tests.
  add_executable(
    test-daily-memory-new
    test/test-daily.cpp
    test/test-daily-memory-new.cpp)
  target_link_libraries(test-daily-memory-new PRIVATE daily::memory_new Boost::unit_test_framework)
  add_test(NAME test-daily-memory-new COMMAND test-daily-memory-new)

  # Separate because it replaces operator new and delete to count
  # allocations.
  add_executable(
    test-daily-future-alloc
    test/test-daily.cpp
    test/test-daily-future-AllocatorSupport.cpp)
  target_link_libraries(test-daily-future-alloc PRIVATE daily::future Boost::unit_test_framework)
  add_test(NAME test-daily-future-alloc COMMAND test-daily-future-alloc)

  # The log tests again with DAILY_ENABLE_FAST_LOG=1. Separate because the
  # setting changes the Logger class, and builds Log.cpp itself rather than
  # linking daily::trace so every translation unit sees the same definition.
  add_executable(
    test-daily-trace-fast-log
    test/test-daily.cpp
    test/test-daily-trace-Log.cpp
    src/Log.cpp)
  target_compile_definitions(test-daily-trace-fast-log PRIVATE DAILY_ENABLE_FAST_LOG=1)
  target_link_libraries(test-daily-trace-fast-log PRIVATE daily::fast_iostream Boost::unit_test_framework)
  add_test(NAME test-daily-trace-fast-log COMMAND test-daily-trace-fast-log)
endif()

option(DAILY_BUILD_BENCHMARKS "Build daily benchmarks" ${PROJECT_IS_TOP_LEVEL})
if(DAILY_BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)

  # Compares FastOStream, FastIStream and the fast locale against the
  # standard streams, std::format and std::charconv.
  add_executable(bench-daily-fast_iostream test/bench-daily-fast_iostream.cpp)
  target_link_libraries(bench-daily-fast_iostream PRIVATE daily::fast_iostream benchmark)
endif()
