cmake_minimum_required(VERSION 2.8)
project (daily)
 
set (PROJECT_INCLUDE_DIR "../")
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
 
file(GLOB_RECURSE  PROJECT_SOURCES *.cpp)
file(GLOB_RECURSE  PROJECT_HEADERS ../*.h)
 
find_package( Boost )

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_INCLUDE_DIR}")

add_library( ${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ) 
target_include_directories( ${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIR} )


