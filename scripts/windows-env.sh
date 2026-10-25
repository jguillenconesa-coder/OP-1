#!/usr/bin/env bash
# Source in a separate shell from the native Linux build environment.
unset CPATH CPLUS_INCLUDE_PATH C_INCLUDE_PATH LIBRARY_PATH PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH
export PYTHONPATH="/workspace/.op1-tools${PYTHONPATH:+:$PYTHONPATH}"
export PATH="/workspace/windows-tools/root/usr/lib/llvm-19/bin:/workspace/.op1-tools/bin:$PATH"
export LD_LIBRARY_PATH="/workspace/windows-tools/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
