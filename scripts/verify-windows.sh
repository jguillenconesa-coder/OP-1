#!/usr/bin/env bash
# Functional checks in our isolated Wine prefix; never touches a user's Windows install.
set -euo pipefail
cd /workspace/OP-1
source scripts/windows-env.sh
export DISPLAY=:77
if [[ ! -S /tmp/.X11-unix/X77 ]]; then
 /workspace/op1-system/root/usr/bin/Xvfb :77 -screen 0 1650x900x24 -nolisten tcp > build/wine-display.log 2>&1 &
 display_pid=$!
 trap 'kill "$display_pid" 2>/dev/null || true' EXIT
 python3 - <<'PY'
from pathlib import Path
import time
for _ in range(100):
    if Path('/tmp/.X11-unix/X77').exists():break
    time.sleep(.05)
else:raise RuntimeError('Virtual display did not start')
PY
fi
scripts/run-wine.sh /workspace/windows-tools/root/usr/lib/x86_64-linux-gnu/wine/x86_64-windows/wineboot.exe -u > build/wine-init.log 2>&1
ctest --test-dir build-win-msvc --output-on-failure
install='/workspace/windows-tools/wine-prefix/drive_c/Program Files/Pocket Instruments/Pocket One'
plugin='/workspace/windows-tools/wine-prefix/drive_c/Program Files/Common Files/VST3/Pocket One.vst3'
if [[ -e "$install" || -e "$plugin" ]]; then echo 'Refusing to overwrite an existing isolated test installation'; exit 1; fi
scripts/run-wine.sh /workspace/OP-1/dist/Pocket-One-0.2.2-Windows-x64-Setup.exe /S > build/installer-install.log 2>&1
python3 - <<'PY'
from pathlib import Path
import hashlib,time
base=Path('/workspace/windows-tools/wine-prefix/drive_c/Program Files');build=Path('/workspace/OP-1/build-win-msvc/PocketOne_artefacts/Release')
pairs=[(build/'Standalone/Pocket One.exe',base/'Pocket Instruments/Pocket One/Pocket One.exe'),(build/'VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3',base/'Common Files/VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3')]
for source,dest in pairs:
    for _ in range(100):
        if dest.exists():break
        time.sleep(.05)
    assert dest.exists(),f'Missing installed file: {dest}'
    assert hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(dest.read_bytes()).digest(),'Installed file hash mismatch'
print('PASS: installed standalone and VST3 hashes match build outputs')
PY
scripts/run-wine.sh /workspace/OP-1/build-win-msvc/windows_module_probe.exe 'C:\Program Files\Common Files\VST3\Pocket One.vst3\Contents\x86_64-win\Pocket One.vst3'
scripts/run-wine.sh reg.exe query 'HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne' /v DisplayVersion > build/installer-registry.log
scripts/run-wine.sh 'C:\Program Files\Pocket Instruments\Pocket One\Uninstall.exe' /S > build/installer-uninstall.log 2>&1
python3 - <<'PY'
from pathlib import Path
import time
base=Path('/workspace/windows-tools/wine-prefix/drive_c/Program Files')
for _ in range(200):
    if not (base/'Pocket Instruments/Pocket One/Pocket One.exe').exists() and not (base/'Common Files/VST3/Pocket One.vst3').exists():break
    time.sleep(.05)
else:raise RuntimeError('Uninstall did not remove product files')
print('PASS: uninstaller removed standalone and VST3 bundle')
PY
if scripts/run-wine.sh reg.exe query 'HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne' > build/installer-uninstall-registry.log 2>&1; then echo 'Uninstall registration remains'; exit 1; fi
echo 'PASS: uninstall registration removed'
