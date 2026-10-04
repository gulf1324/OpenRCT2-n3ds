#!/usr/bin/env bash
# 빌드 보조 도구(CMake 3.31, ninja, GNU make)를 tools/host 아래에만 설치한다.
# CMake 4.x는 cmake_minimum_required(VERSION 2.6)인 v0.0.5 스크립트를 거부하므로 3.31 계열을 쓴다.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOST="$ROOT/tools/host"; CACHE="$ROOT/tools/.cache/host"
CMAKE_VER=3.31.12; NINJA_VER=1.13.2; MAKE_PKG=make-4.4.1-3-x86_64.pkg.tar.zst
mkdir -p "$HOST/bin" "$CACHE"
get() { [ -f "$CACHE/$2" ] || { echo "download $2"; curl -sSfL -o "$CACHE/$2.part" "$1"; mv "$CACHE/$2.part" "$CACHE/$2"; }; }

get "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-windows-x86_64.zip" cmake.zip
rm -rf "$HOST/cmake"; unzip -q "$CACHE/cmake.zip" -d "$HOST" && mv "$HOST/cmake-$CMAKE_VER-windows-x86_64" "$HOST/cmake"

get "https://github.com/ninja-build/ninja/releases/download/v$NINJA_VER/ninja-win.zip" ninja.zip
unzip -qo "$CACHE/ninja.zip" -d "$HOST/bin"

# msys2의 make. Git Bash의 msys 런타임으로 실행된다.
get "https://repo.msys2.org/msys/x86_64/$MAKE_PKG" "$MAKE_PKG"
tar --zstd -xf "$CACHE/$MAKE_PKG" -C "$CACHE" usr/bin/make.exe
cp "$CACHE/usr/bin/make.exe" "$HOST/bin/make.exe"
echo "done: $HOST"
