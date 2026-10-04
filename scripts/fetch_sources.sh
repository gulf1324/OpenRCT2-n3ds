#!/usr/bin/env bash
# Fetches the libraries that have no 3DS package into external/; scripts/build_deps.sh builds them.
# (The game itself, OpenRCT2 with the port, is in this repository: external/OpenRCT2.)
# Run again to fetch only what is missing. Line-ending conversion is turned off per repository;
# the global git settings are not touched.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
clone() { # url tag dest
  local url=$1 tag=$2 dest="$ROOT/$3"
  if [ -d "$dest/.git" ]; then echo "exists: $3"; return; fi
  echo "clone $url @ $tag -> $3"
  git -c core.autocrlf=false -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" "$url" "$dest"
  git -C "$dest" config core.autocrlf false
}
clone https://github.com/libsdl-org/SDL.git   release-2.32.10 external/SDL2
clone https://github.com/nih-at/libzip.git    v1.11.4         external/libzip
clone https://github.com/xiph/speexdsp.git    SpeexDSP-1.2.1  external/speexdsp
echo done
