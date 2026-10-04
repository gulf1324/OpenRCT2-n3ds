# speexdsp는 autotools만 제공하므로, 3DS용 정적 라이브러리를 만들기 위한 최소 CMake 빌드.
# 원본 소스(external/speexdsp)는 수정하지 않는다. 설정값은 configure의 기본값과 같다:
# 부동소수점 모드, FFT는 smallft.
cmake_minimum_required(VERSION 3.13)
project(speexdsp VERSION 1.2.1 LANGUAGES C)

set(SPEEXDSP_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../external/speexdsp" CACHE PATH "speexdsp source")
get_filename_component(SPEEXDSP_SRC "${SPEEXDSP_SRC}" ABSOLUTE)

set(GEN_INC "${CMAKE_CURRENT_BINARY_DIR}/include")
file(WRITE "${GEN_INC}/speex/speexdsp_config_types.h"
"#ifndef __SPEEX_TYPES_H__\n#define __SPEEX_TYPES_H__\n\n#include <stdint.h>\n\n"
"typedef int16_t spx_int16_t;\ntypedef uint16_t spx_uint16_t;\n"
"typedef int32_t spx_int32_t;\ntypedef uint32_t spx_uint32_t;\n\n#endif\n")

set(L "${SPEEXDSP_SRC}/libspeexdsp")
add_library(speexdsp STATIC
  ${L}/buffer.c ${L}/fftwrap.c ${L}/filterbank.c ${L}/jitter.c ${L}/mdf.c
  ${L}/preprocess.c ${L}/resample.c ${L}/scal.c ${L}/smallft.c)
target_include_directories(speexdsp PUBLIC "${SPEEXDSP_SRC}/include" "${GEN_INC}" "${GEN_INC}/speex")
target_compile_definitions(speexdsp PRIVATE FLOATING_POINT USE_SMALLFT EXPORT=)

include(GNUInstallDirs)
install(TARGETS speexdsp ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
  ${SPEEXDSP_SRC}/include/speex/speex_buffer.h
  ${SPEEXDSP_SRC}/include/speex/speex_echo.h
  ${SPEEXDSP_SRC}/include/speex/speex_jitter.h
  ${SPEEXDSP_SRC}/include/speex/speex_preprocess.h
  ${SPEEXDSP_SRC}/include/speex/speex_resampler.h
  ${SPEEXDSP_SRC}/include/speex/speexdsp_types.h
  ${GEN_INC}/speex/speexdsp_config_types.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/speex)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/speexdsp.pc"
"prefix=${CMAKE_INSTALL_PREFIX}\nlibdir=\${prefix}/lib\nincludedir=\${prefix}/include\n\n"
"Name: speexdsp\nDescription: Speex DSP library\nVersion: ${PROJECT_VERSION}\n"
"Libs: -L\${libdir} -lspeexdsp\nLibs.private: -lm\nCflags: -I\${includedir}\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/speexdsp.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
