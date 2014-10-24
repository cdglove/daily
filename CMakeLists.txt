cmake_minimum_required(VERSION 2.8)
project(daily)
# Avoid broken cmake versioning...
set(VER_MAJ 1)
set(VER_MIN 0)
set(VER_STR 1.0)

configure_file (
  "${PROJECT_SOURCE_DIR}/config/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )
 
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INCLUDE_DIR ".")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
	if(NOT MSVC)
		message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has may not support Please use a different C++ compiler if building fails.")
	endif()
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(test)
