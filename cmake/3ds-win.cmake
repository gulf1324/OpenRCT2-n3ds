# Windows 네이티브 CMake용 3DS 툴체인 래퍼.
# devkitPro의 3DS.cmake와 내용이 같고, msys2 CMake만 허용하는 호스트 검사만 뺐다.
# devkitPro 파일은 tools/ 아래에서 그대로 두고 수정하지 않는다.
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{DEVKITPRO})
	message(FATAL_ERROR "DEVKITPRO가 없습니다. source ./env.sh 후 실행하세요")
endif()
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" DEVKITPRO)
# Git Bash 경로(/e/...)를 Windows 경로(E:/...)로 바꾼다
if(DEVKITPRO MATCHES "^/([a-zA-Z])/(.*)$")
	set(DEVKITPRO "${CMAKE_MATCH_1}:/${CMAKE_MATCH_2}")
endif()
set(ENV{DEVKITPRO} "${DEVKITPRO}")
set(ENV{DEVKITARM} "${DEVKITPRO}/devkitARM")
list(APPEND CMAKE_MODULE_PATH "${DEVKITPRO}/cmake")

if(NOT CMAKE_SYSTEM_NAME)
	set(CMAKE_SYSTEM_NAME Nintendo3DS)
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR)
	set(CMAKE_SYSTEM_PROCESSOR armv6k)
endif()

# devkitARM.cmake에서 dkp-initialize-path 를 뺀 부분
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${DEVKITPRO}/cmake/dkp-rule-overrides.cmake")
include(dkp-toolchain-common)
set(DKP_BIN2S_ALIGNMENT 4)
__dkp_toolchain(devkitARM arm arm-none-eabi)

# 3DS.cmake 나머지
set(CTR_ROOT ${DEVKITPRO}/libctru)
set(DKP_INSTALL_PREFIX_INIT ${DEVKITPRO}/portlibs/3ds)
__dkp_platform_prefix(
	${DEVKITPRO}/portlibs/3ds
	${CTR_ROOT}
)

# pkg-config 래퍼는 bash 스크립트라 Windows CMake가 직접 실행하지 못한다. 필요해지면 따로 연결한다
find_program(PKG_CONFIG_EXECUTABLE NAMES arm-none-eabi-pkg-config.cmd HINTS "${CMAKE_CURRENT_LIST_DIR}/../tools/host/bin")

find_program(CTR_SMDHTOOL_EXE NAMES smdhtool HINTS "${DEVKITPRO}/tools/bin")
find_program(CTR_3DSXTOOL_EXE NAMES 3dsxtool HINTS "${DEVKITPRO}/tools/bin")
find_program(CTR_PICASSO_EXE NAMES picasso HINTS "${DEVKITPRO}/tools/bin")
find_program(CTR_TEX3DS_EXE NAMES tex3ds HINTS "${DEVKITPRO}/tools/bin")
find_file(CTR_DEFAULT_ICON NAMES default_icon.png HINTS "${CTR_ROOT}" NO_CMAKE_FIND_ROOT_PATH)
