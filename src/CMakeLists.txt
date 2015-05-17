cmake_minimum_required(VERSION 2.8)
project (daily)
 
file(GLOB_RECURSE  PROJECT_SOURCES *.cpp)
file(GLOB_RECURSE  PROJECT_HEADERS ../*.h)
 
find_package( Boost )

add_library( ${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ) 
target_include_directories( ${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIR} ${DAILY_INCLUDE_DIRS})


