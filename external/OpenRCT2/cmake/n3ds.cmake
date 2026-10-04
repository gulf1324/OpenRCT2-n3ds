# Nintendo 3DS 빌드 (n3ds 포팅 전용).
# CMakeLists.txt에서 CMAKE_SYSTEM_NAME이 Nintendo3DS일 때만 include 된다.
# 원래의 리눅스/맥 빌드 로직은 pkg-config에 의존해 Windows용 CMake에서 돌지 않으므로,
# 그 로직은 건드리지 않고 여기서 3DS 빌드만 따로 정의한다.
cmake_policy(VERSION 3.13)

# 3DS에서 끄는 기능. 네트워크·HTTP·Twitch는 libcurl/openssl이 없고, OpenGL은 3DS에 없다.
# TTF는 src/platform/n3ds/include/SDL_ttf.h 가 항상 실패하는 대체 구현을 제공해 스프라이트 폰트로 돌아가게 한다.
add_definitions(-DDISABLE_NETWORK -DDISABLE_HTTP -DDISABLE_TWITCH -DDISABLE_OPENGL)
add_definitions(-DDEBUG=0)

# 타이틀 화면의 로고. 기본은 OpenRCT2의 로고다. ON이면 RollerCoaster Tycoon 2의 로고를 사용자의 g1.dat에서 그린다
# (src/windows/title_logo.c). 자기 기기에서만 쓰는 빌드용이다: 남에게 주는 빌드는 OpenRCT2의 로고로 둔다.
option(N3DS_RCT2_TITLE_LOGO "Title screen: the RollerCoaster Tycoon 2 logo from g1.dat instead of OpenRCT2's" OFF)
if (N3DS_RCT2_TITLE_LOGO)
	add_definitions(-DN3DS_RCT2_TITLE_LOGO)
endif ()

# x86에서 char는 부호가 있고 ARM에서는 없다. 원본 코드는 x86 동작을 가정하므로 맞춰 준다.
# 반환값이 빠진 함수는 플랫폼 분기가 빠졌다는 신호라 오류로 처리한다 (FileScanner 사례)
# -fno-strict-aliasing: 디컴파일된 코드는 한 메모리를 여러 타입으로 읽는 곳이 많다(구조체 위의 uint8/uint16
# 캐스팅). -O2는 타입이 다르면 같은 메모리가 아니라고 가정해 최적화하므로 그런 코드가 잘못 컴파일될 수 있다.
# UPSTREAM_REVIEW.md B-2. 에뮬레이터 타이틀 화면에서 전후 차이 없음(명령 수 기준)
set(N3DS_COMMON_FLAGS "-fsigned-char -fno-strict-aliasing -Werror=return-type -Wall -Wno-unknown-pragmas -Wno-unused-function -Wno-missing-braces -Wno-comment")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 ${N3DS_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 ${N3DS_COMMON_FLAGS}")

file(GLOB_RECURSE ORCT2_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

find_package(SDL2 REQUIRED CONFIG)
find_package(libzip REQUIRED CONFIG)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(${PROJECT} ${ORCT2_SOURCES})
target_include_directories(${PROJECT} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src/platform/n3ds/include"
	"${DEVKITPRO}/portlibs/3ds/include")
target_link_libraries(${PROJECT} PRIVATE
	SDL2::SDL2main SDL2::SDL2-static libzip::zip
	PNG::PNG ZLIB::ZLIB
	"${DEVKITPRO}/portlibs/3ds/lib/libjansson.a"
	"${DEVKITPRO}/portlibs/3ds/lib/libspeexdsp.a"
	"${DEVKITPRO}/portlibs/3ds/lib/libiconv.a"
	m)

# The name of the build (src/version.c): made at every build by cmake/n3ds_build_id.cmake.
# The commit hash CMakeLists.txt defines is that of the last time CMake configured.
add_custom_target(n3ds_build_id
	COMMAND ${CMAKE_COMMAND}
		"-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
		"-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/n3ds_build_id.h"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/n3ds_build_id.cmake"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/n3ds_build_id.h"
	VERBATIM)
add_dependencies(${PROJECT} n3ds_build_id)
target_include_directories(${PROJECT} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Every fopen goes through __wrap_fopen (src/platform/n3ds.c) to get a large stdio buffer
target_link_options(${PROJECT} PRIVATE "-Wl,--wrap=fopen")

ctr_generate_smdh(${PROJECT}.smdh
	NAME "OpenRCT2"
	DESCRIPTION "OpenRCT2 v0.0.5 for New 3DS"
	AUTHOR "OpenRCT2 developers, n3ds port")
ctr_create_3dsx(${PROJECT} SMDH ${PROJECT}.smdh)
