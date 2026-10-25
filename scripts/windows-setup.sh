#!/usr/bin/env bash
# Reproduce the verified clang-cl/Microsoft SDK cross build in Codex Debian 13.
set -euo pipefail
cd /workspace/OP-1
source scripts/cloud-env.sh
if [[ ! -x /workspace/.op1-tools/bin/ninja ]]; then python3 -m pip install --target /workspace/.op1-tools ninja==1.13.2; fi
mkdir -p /workspace/windows-tools/debs /workspace/windows-tools/root
apt_options=(-o Dir::Etc::parts=- -o Dir::Etc::sourcelist=/workspace/op1-system/sources.list -o Dir::Etc::sourceparts=- -o Dir::State::lists=/workspace/op1-system/lists)
(
 cd /workspace/windows-tools/debs
 apt-get "${apt_options[@]}" download clang-19/trixie lld-19/trixie libllvm19/trixie libclang-cpp19/trixie libclang1-19/trixie libclang-common-19-dev/trixie libz3-4/trixie llvm-19/trixie llvm-19-linker-tools/trixie llvm-19-runtime/trixie nsis/trixie nsis-common/trixie wine64/trixie libwine/trixie libz-mingw-w64/trixie
)
python3 - <<'PY'
from pathlib import Path
import subprocess,urllib.request,hashlib,tarfile,shutil
root=Path('/workspace/windows-tools/root')
prefixes=('clang-','lld-','libllvm','libclang','libz3-','llvm-','nsis','wine64_','libwine_','libz-mingw')
for p in Path('/workspace/windows-tools/debs').glob('*.deb'):
    if p.name.startswith(prefixes):subprocess.run(['dpkg-deb','-x',str(p),str(root)],check=True)
clang=root/'usr/lib/llvm-19/bin/clang-cl'
if not clang.exists():clang.symlink_to('clang')
xwin=Path('/workspace/windows-tools/xwin-0.10.0-x86_64-unknown-linux-musl/xwin')
if not xwin.exists():
    base='https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz'
    with urllib.request.urlopen(base,timeout=60) as r:data=r.read()
    with urllib.request.urlopen(base+'.sha256',timeout=30) as r:expected=r.read().decode().strip().split()[0].lower()
    if hashlib.sha256(data).hexdigest()!=expected:raise RuntimeError('xwin checksum mismatch')
    archive=Path('/workspace/windows-tools/xwin.tar.gz');archive.write_bytes(data)
    with tarfile.open(archive) as tar:tar.extractall('/workspace/windows-tools',filter='data')
# Relocate Debian's Wine launcher/data paths inside the workspace.
server=root/'usr/lib/wine/wineserver'
if not server.is_symlink():server.unlink();server.symlink_to('wineserver64')
share=root/'usr/share/wine';alias=share/'wine';alias.mkdir(exist_ok=True)
for p in share.iterdir():
    if p.name!='wine' and not (alias/p.name).exists():(alias/p.name).symlink_to(Path('..')/p.name)
zlib=root/'usr/lib/x86_64-linux-gnu/wine/x86_64-windows/zlib1.dll'
if not zlib.exists():zlib.symlink_to(root/'usr/x86_64-w64-mingw32/lib/zlib1.dll')
# Seed the isolated prefix with verified modules before Wine's first boot.
system=Path('/workspace/windows-tools/wine-prefix/drive_c/windows/system32');system.mkdir(parents=True,exist_ok=True)
for p in (root/'usr/lib/x86_64-linux-gnu/wine/x86_64-windows').iterdir():
    if p.is_file() and p.suffix in ('.dll','.exe') and not (system/p.name).exists():shutil.copyfile(p,system/p.name)
PY
if [[ ! -d /workspace/windows-tools/msvc-sdk/crt/include ]]; then
 /workspace/windows-tools/xwin-0.10.0-x86_64-unknown-linux-musl/xwin --accept-license --cache-dir /workspace/windows-tools/xwin-cache splat --output /workspace/windows-tools/msvc-sdk
fi
# JUCE's installed package exports a host juceaide, used while cross compiling.
cmake -S /workspace/juce-sdk -B /workspace/juce-host-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/workspace/juce-installed
cmake --build /workspace/juce-host-build --parallel 2
cmake --install /workspace/juce-host-build
python3 scripts/windows-case-aliases.py
source scripts/windows-env.sh
cmake -S . -B build-win-msvc -G Ninja -DCMAKE_MAKE_PROGRAM=/workspace/.op1-tools/bin/ninja -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-x64.cmake -DJUCE_PACKAGE_PATH=/workspace/juce-installed/lib/cmake/JUCE-8.0.6 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DJUCE_TARGET_ARCHITECTURE=x86_64 -DCMAKE_CROSSCOMPILING_EMULATOR=/workspace/OP-1/scripts/run-wine.sh
cmake --build build-win-msvc --parallel 3
mkdir -p dist
NSISDIR=/workspace/windows-tools/root/usr/share/nsis /workspace/windows-tools/root/usr/bin/makensis -V3 -DPOCKET_AMD64_INSTALLER=1 -DARTIFACT_DIR=/workspace/OP-1/build-win-msvc/PocketOne_artefacts/Release -DSOURCE_DIR=/workspace/OP-1 -DOUTPUT_DIR=/workspace/OP-1/dist packaging/windows/PocketOne.nsi
