cmake_minimum_required(VERSION 2.8)
project (daily_test)
 
set (PROJECT_INCLUDE_DIR "../")
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
 
find_package( Boost )

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_INCLUDE_DIR}")

## --- Unit Testing --- ##
enable_testing()

add_executable(test_fast_iostream fast_iostream/io.cpp)
target_link_libraries(test_fast_iostream daily)
target_include_directories( test_fast_iostream PUBLIC ${Boost_INCLUDE_DIR} )
add_test(test_fast_iostream test_fast_iostream)

add_executable(test_trace trace/basic.cpp)
target_link_libraries(test_trace daily)
target_include_directories( test_trace PUBLIC ${Boost_INCLUDE_DIR} )
add_test(test_trace test_trace)


