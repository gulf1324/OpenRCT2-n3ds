#!/usr/bin/env bash
# .3dsx를 3DS로 무선 전송해 실행한다. 3DS에서 Homebrew Launcher를 열고 Y를 누른 상태여야 한다.
# 사용법: scripts/send.sh path/to/app.3dsx
# 3DS IP는 local.env의 N3DS_IP로 지정한다. 없으면 브로드캐스트로 찾는다.
# -s 옵션으로 3DS의 printf 출력이 PC 터미널로 돌아온다 (앱에서 link3dsStdio() 호출 필요).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/env.sh"
[ -f "$ROOT/local.env" ] && source "$ROOT/local.env"
args=(-s)
[ -n "${N3DS_IP:-}" ] && args+=(-a "$N3DS_IP")
exec 3dslink "${args[@]}" "$@"
