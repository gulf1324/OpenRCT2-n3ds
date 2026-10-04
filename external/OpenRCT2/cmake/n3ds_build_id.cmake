# n3ds port: names the build for the version text on the title screen and for the log.
# Run at every build (cmake/n3ds.cmake), not when CMake configures: the name is the commit the
# repository is at, with a "+" when there are uncommitted changes to tracked files of the game's
# sources (this folder: the repository around it also holds documents and scripts, which do not
# change the build). The header is only rewritten when the name changes, so nothing is
# recompiled otherwise.
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${SOURCE_DIR}
	OUTPUT_VARIABLE buildId
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
execute_process(
	COMMAND git status --porcelain --untracked-files=no -- .
	WORKING_DIRECTORY ${SOURCE_DIR}
	OUTPUT_VARIABLE changes
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
if(NOT "${changes}" STREQUAL "")
	set(buildId "${buildId}+")
endif()

set(content "#define N3DS_BUILD_ID \"${buildId}\"\n")
set(old "")
if(EXISTS "${OUTPUT}")
	file(READ "${OUTPUT}" old)
endif()
if(NOT "${old}" STREQUAL "${content}")
	file(WRITE "${OUTPUT}" "${content}")
endif()
