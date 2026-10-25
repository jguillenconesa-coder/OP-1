#!/usr/bin/env bash
set -euo pipefail
export WINEPREFIX=/workspace/windows-tools/wine-prefix
export WINEDLLPATH=/workspace/windows-tools/root/usr/lib/x86_64-linux-gnu/wine
export WINEDEBUG="${WINEDEBUG:--all}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/workspace/.cache}"
export DISPLAY="${DISPLAY:-:77}"
exec /workspace/windows-tools/root/usr/lib/wine/wine64 "$@"
