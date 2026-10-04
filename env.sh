# 프로젝트 전용 개발 환경. 시스템 설정은 바꾸지 않고 현재 셸에만 적용된다.
# 사용법 (Git Bash): source ./env.sh
_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export DEVKITPRO="$_ROOT/tools/devkitpro"
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$_ROOT/tools/host/bin:$_ROOT/tools/host/cmake/bin:$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
export RCT2_DATA="$_ROOT/gamedata/rct2"
export RCT1_DATA="$_ROOT/gamedata/rct1"
unset _ROOT
