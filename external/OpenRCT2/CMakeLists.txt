cmake_minimum_required(VERSION 3.13)

# n3ds port: this tree builds for the New 3DS only. The PC build of OpenRCT2 0.0.5 that was here was
# dropped in October 2026 (CLAUDE.md "기능 정리"). Configure with the 3DS toolchain file of the project
# above this folder (cmake/3ds-win.cmake); the build itself is defined in cmake/n3ds.cmake.

set(PROJECT openrct2)
project(${PROJECT})

if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
	message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS")
	message(FATAL_ERROR "This tree builds for the Nintendo 3DS only: configure with the 3DS toolchain file")
endif()

# The commit hash of the configure (src/version.c). The name shown by the game is that of the commit
# at build time (cmake/n3ds_build_id.cmake, included from cmake/n3ds.cmake).
execute_process(
	COMMAND git rev-parse HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE OPENRCT2_COMMIT_SHA1
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
add_definitions(-DOPENRCT2_COMMIT_SHA1="${OPENRCT2_COMMIT_SHA1}")
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE OPENRCT2_COMMIT_SHA1_SHORT
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
add_definitions(-DOPENRCT2_COMMIT_SHA1_SHORT="${OPENRCT2_COMMIT_SHA1_SHORT}")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/n3ds.cmake)
