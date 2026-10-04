#!/usr/bin/env bash
# devkitPro 3DS 툴체인을 프로젝트 안(tools/devkitpro)에만 설치한다.
# 공식 설치기는 C:\devkitPro와 시스템 환경변수를 바꾸므로 쓰지 않고,
# pacman 패키지 파일을 직접 받아 압축만 푼다. 다시 실행해도 안전하다.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DKP="$ROOT/tools/devkitpro"
CACHE="$ROOT/tools/.cache/dkp"
UA="pacman/6.0.2 (MSYS_NT-10.0 x86_64) libalpm/13.0.2"
BASE="https://pkg.devkitpro.org/packages"
mkdir -p "$DKP" "$CACHE/db/libs" "$CACHE/db/win"

curl -sSfL -A "$UA" -o "$CACHE/libs.db" "$BASE/dkp-libs.db"
curl -sSfL -A "$UA" -o "$CACHE/win.db"  "$BASE/windows/x86_64/dkp-windows.db"
tar xf "$CACHE/libs.db" -C "$CACHE/db/libs"
tar xf "$CACHE/win.db"  -C "$CACHE/db/win"

# 3ds-dev 묶음 + 이 프로젝트가 쓰는 portlibs
( cd "$CACHE" && PLAN_OUT="$CACHE/plan.json" python "$ROOT/scripts/dkp_resolve.py" \
    3ds-jansson 3ds-libpng 3ds-zlib 3ds-libiconv 3ds-pkg-config devkitARM-gdb )

python -c "import json,sys;[print(r,f) for r,f in json.load(open(sys.argv[1]))]" "$CACHE/plan.json" \
  | tr -d '\r' > "$CACHE/plan.txt"

while read -r repo file; do
  url="$BASE/$file"
  [ "$repo" = win ] && url="$BASE/windows/x86_64/$file"
  if [ ! -f "$CACHE/$file" ]; then
    echo "download $file"
    curl -sSfL -A "$UA" -o "$CACHE/$file.part" "$url"
    mv "$CACHE/$file.part" "$CACHE/$file"
  fi
  case "$file" in *.zst) comp=--zstd ;; *.xz) comp=-J ;; esac
  # 메타 패키지처럼 opt/devkitpro 아래 파일이 없는 패키지는 건너뛴다
  n=$(tar $comp -tf "$CACHE/$file" 2>/dev/null | grep -c '^opt/devkitpro/.' || true)
  if [ "$n" -gt 0 ]; then
    tar $comp -xf "$CACHE/$file" -C "$DKP" --strip-components=2 --wildcards 'opt/devkitpro/*' 2>&1       | grep -v "hdrcharset" || true
  else
    echo "skip (no files): $file"
  fi
done < "$CACHE/plan.txt"
echo "done: $DKP"
