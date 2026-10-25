#!/usr/bin/env bash
# Source this file in the prepared Codex Debian environment.
export PYTHONPATH="/workspace/.op1-tools${PYTHONPATH:+:$PYTHONPATH}"
export PATH="/workspace/.op1-tools/bin:$PATH"
export CPATH="/workspace/op1-system/root/usr/include:/workspace/op1-system/root/usr/include/freetype2${CPATH:+:$CPATH}"
export LIBRARY_PATH="/workspace/op1-system/root/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
export PKG_CONFIG_SYSROOT_DIR=/workspace/op1-system/root
export PKG_CONFIG_PATH="/workspace/op1-system/root/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/op1-system/root/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/workspace/.cache}"
mkdir -p "$XDG_CACHE_HOME"
