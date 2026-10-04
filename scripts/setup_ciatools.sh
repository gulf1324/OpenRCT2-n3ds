#!/usr/bin/env bash
# CIA(3DS 홈 메뉴에 설치하는 파일)를 만드는 도구를 tools/host/bin 아래에만 설치한다.
#   makerom    : .elf + 설정(.rsf) + 아이콘 + 배너 -> .cia  (3DSGuy/Project_CTR)
#   bannertool : 그림 + 소리 -> 홈 메뉴 위 화면의 배너(.bnr)   (carstene1ns/3ds-bannertool)
# devkitPro 패키지에는 이 둘이 없다. CIA 만들기는 scripts/make_cia.py
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOST="$ROOT/tools/host"; CACHE="$ROOT/tools/.cache/host"
MAKEROM_VER=0.19.0; BANNERTOOL_VER=1.2.3
mkdir -p "$HOST/bin" "$CACHE"
get() { [ -f "$CACHE/$2" ] || { echo "download $2"; curl -sSfL -o "$CACHE/$2.part" "$1"; mv "$CACHE/$2.part" "$CACHE/$2"; }; }

get "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v$MAKEROM_VER/makerom-v$MAKEROM_VER-win_x86_64.zip" "makerom-$MAKEROM_VER.zip"
get "https://github.com/carstene1ns/3ds-bannertool/releases/download/$BANNERTOOL_VER/bannertool-$BANNERTOOL_VER-windows.zip" "bannertool-$BANNERTOOL_VER.zip"

# 압축 파일 안의 폴더 구조와 상관없이 실행 파일(과 같이 든 DLL)만 꺼낸다
for name in makerom bannertool; do
	ver_var="$(echo "$name" | tr a-z A-Z)_VER"
	tmp="$CACHE/$name-unzip"; rm -rf "$tmp"; mkdir -p "$tmp"
	unzip -qo "$CACHE/$name-${!ver_var}.zip" -d "$tmp"
	exe="$(find "$tmp" -iname "$name.exe" | head -1)"
	[ -n "$exe" ] || { echo "$name.exe not found in the archive"; exit 1; }
	cp "$exe" "$HOST/bin/"
	find "$(dirname "$exe")" -maxdepth 1 -iname '*.dll' -exec cp {} "$HOST/bin/" \;
	rm -rf "$tmp"
done
echo "done: $HOST/bin/makerom.exe, $HOST/bin/bannertool.exe"
