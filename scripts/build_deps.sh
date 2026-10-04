#!/usr/bin/env bash
# devkitPro 패키지가 없는 라이브러리(SDL2, libzip, speexdsp)를 3DS용으로 빌드해
# tools/devkitpro/portlibs/3ds 에 설치한다. setup_toolchain.sh, setup_hosttools.sh, fetch_sources.sh 다음에 실행.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"; source ./env.sh
TC="-DCMAKE_TOOLCHAIN_FILE=$ROOT/cmake/3ds-win.cmake -DCMAKE_BUILD_TYPE=Release -G Ninja"
B="$ROOT/build/deps"

cmake -S external/SDL2 -B "$B/sdl2" $TC -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
cmake --build "$B/sdl2" && cmake --install "$B/sdl2"

cmake -S external/libzip -B "$B/libzip" $TC -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF \
  -DENABLE_OPENSSL=OFF -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_COMMONCRYPTO=OFF -DENABLE_WINDOWS_CRYPTO=OFF \
  -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF -DBUILD_OSSFUZZ=OFF
cmake --build "$B/libzip" && cmake --install "$B/libzip"

cmake -S cmake/deps/speexdsp -B "$B/speexdsp" $TC
cmake --build "$B/speexdsp" && cmake --install "$B/speexdsp"
echo "done"
