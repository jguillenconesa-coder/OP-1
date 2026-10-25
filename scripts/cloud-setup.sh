#!/usr/bin/env bash
# Workspace-local dependencies for the read-only Codex Debian 13 machine.
set -euo pipefail
cd /workspace/OP-1
if [[ ! -x /workspace/.op1-tools/bin/cmake ]]; then
  python3 -m pip install --target /workspace/.op1-tools cmake==4.4.4
fi
mkdir -p /workspace/op1-system/lists/partial /workspace/op1-system/debs /workspace/op1-system/root
cat > /workspace/op1-system/sources.list <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
apt_options=(-o Dir::Etc::parts=- -o Dir::Etc::sourcelist=/workspace/op1-system/sources.list -o Dir::Etc::sourceparts=- -o Dir::State::lists=/workspace/op1-system/lists)
apt-get "${apt_options[@]}" update
(
 cd /workspace/op1-system/debs
 packages=(libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev
  libxrender-dev libxfixes-dev libfreetype-dev libfontconfig-dev libpng-dev libbrotli-dev
  libbz2-dev libxft-dev libxcomposite-dev libxi-dev libgl-dev libglx-dev libxkbcommon-dev
  x11proto-dev xtrans-dev libxcb1-dev libxau-dev libxdmcp-dev
  xvfb xserver-common libxfont2 x11-xkb-utils libxkbfile1)
 selected=(); for package in "${packages[@]}"; do selected+=("$package/trixie"); done
 apt-get "${apt_options[@]}" download "${selected[@]}"
)
python3 - <<'PY'
import pathlib, subprocess, os
root = pathlib.Path('/workspace/op1-system/root')
for package in pathlib.Path('/workspace/op1-system/debs').glob('*.deb'):
    subprocess.run(['dpkg-deb', '-x', str(package), str(root)], check=True)
# Development packages contain relative .so links. Reuse the machine's runtime
# libraries when the corresponding target is not part of the extracted packages.
for item in root.rglob('*'):
    if item.is_symlink() and not item.exists():
        target = pathlib.Path(os.readlink(item))
        system = target if target.is_absolute() else pathlib.Path('/') / item.relative_to(root).parent / target
        if system.exists():
            item.unlink()
            item.symlink_to(system.resolve())
PY
if [[ ! -d /workspace/juce-sdk/.git ]]; then
 git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git /workspace/juce-sdk
fi
[[ "$(git -C /workspace/juce-sdk rev-parse HEAD)" == 51a8a6d7aeae7326956d747737ccf1575e61e209 ]] || {
 echo 'Existing JUCE checkout is not the required version. Supply JUCE 8.0.6 without resetting other work.' >&2; exit 1;
}
source scripts/cloud-env.sh
cmake -S . -B build -DJUCE_PATH=/workspace/juce-sdk -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/workspace/op1-system/root/usr
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
