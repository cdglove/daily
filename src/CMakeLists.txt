cmake_minimum_required(VERSION 2.8)
project (daily)
 
file(GLOB_RECURSE  PROJECT_SOURCES *.cpp)
file(GLOB_RECURSE  PROJECT_HEADERS ../*.h)
 
find_package( Boost )

add_library( ${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ) 
target_include_directories( ${PROJECT_NAME} PUBLIC ${DAILY_INCLUDE_DIRS})
target_include_directories( ${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})


