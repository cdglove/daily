cmake_minimum_required(VERSION 2.8)
project (daily)
set (daily_VERSION_MAJOR 0)
set (daily_VERSION_MINOR 1)

configure_file (
  "${PROJECT_SOURCE_DIR}/config/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )
 
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INCLUDE_DIR ".")

enable_testing()

add_subdirectory(src)
add_subdirectory(test)





