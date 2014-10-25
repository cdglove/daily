cmake_minimum_required(VERSION 2.8)
project(daily)

set(DAILY_VERSION_MAJOR 1)
set(DAILY_VERSION_MINOR 0)
set(DAILY_VERSION_PATCH 0)
set(DAILY_VERSION_STRING ${DAILY_VERSION_MAJOR}.${DAILY_VERSION_MINOR}.${DAILY_VERSION_PATCH})
message("Daily Version: ${DAILY_VERSION_STRING}")
 
option( DAILY_BUILD_TESTS "Build the daily project tests." ON)

include(GNUInstallDirs)

# determine install path for CMake config files
if(WIN32 AND NOT CYGWIN)
	set(DEF_INSTALL_CMAKE_DIR CMake)
else()
	set(DEF_INSTALL_CMAKE_DIR lib/cmake/cppnetlib)
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative cmake install path absolute (needed later on)
if(NOT IS_ABSOLUTE "${INSTALL_CMAKE_DIR}")
	set(INSTALL_CMAKE_DIR "${CMAKE_INSTALL_PREFIX}/${INSTALL_CMAKE_DIR}")
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/config/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h" )
 
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
if(DAILY_BUILD_TESTS)
	add_subdirectory(test)
endif()

###
## Export Targets
# (so cpp-netlib can be easily used by other CMake projects)
# [see http://www.cmake.org/Wiki/CMake/Tutorials/How_to_create_a_ProjectConfig.cmake_file]

# Add all targets to the build-tree export set
export(TARGETS daily
	FILE "${PROJECT_BINARY_DIR}/dailyTargets.cmake")
# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE daily)
# Create the dailyConfig.cmake and dailyConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}" "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}")
configure_file(dailyConfig.cmake.in
	"${PROJECT_BINARY_DIR}/dailyConfig.cmake" @ONLY)
# ... for the install tree
set(CONF_INCLUDE_DIRS "\${DAILY_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(dailyConfig.cmake.in
	"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/dailyConfig.cmake" @ONLY)
# ... for both
configure_file(dailyConfigVersion.cmake.in
    "${PROJECT_BINARY_DIR}/dailyConfigVersion.cmake" @ONLY)
# Install the dailyConfig.cmake and cppnetlibConfigVersion.cmake
install(FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/dailyConfig.cmake"
    "${PROJECT_BINARY_DIR}/dailyConfigVersion.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev)
# Install the export set for use with the install-tree
# install(EXPORT dailyTargets
#     DESTINATION "${INSTALL_CMAKE_DIR}"
#     COMPONENT dev)