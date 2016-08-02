cmake_minimum_required(VERSION 3.5)

option( DAILY_BUILD_TESTS "Build Daily Unit Tests" ON )
enable_testing()

find_package(Boost)

add_subdirectory(libs/fast_iostream)
add_subdirectory(libs/future)
add_subdirectory(libs/memory)
add_subdirectory(libs/program_options)
add_subdirectory(libs/trace)