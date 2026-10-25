#!/usr/bin/env python3
"""Package the verified binaries and corresponding source for a local release.

Run from any directory after both native and Windows builds and NSIS packaging.
Nothing is uploaded by this script. It does not include SDK caches or credentials.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tarfile
import zipfile

root = Path(__file__).resolve().parents[1]
version = re.search(r'project\(PocketOne VERSION ([0-9.]+)', (root/'CMakeLists.txt').read_text()).group(1)
dist = root / 'dist'
output = root / f'downloads/v{version}'
output.mkdir(parents=True, exist_ok=True)
required = [dist/f'Pocket-One-{version}-Windows-x64-Setup.exe',
            root/'build-win-msvc/PocketOne_artefacts/Release/Standalone/Pocket One.exe',
            root/'build/PocketOne_artefacts/Release/Standalone/Pocket One']
for item in required:
    if not item.is_file():
        raise RuntimeError(f'Missing build product: {item}')
juce = Path('/workspace/juce-sdk')
revision = subprocess.check_output(['git', '-C', str(juce), 'rev-parse', 'HEAD'], text=True).strip()
if revision != '51a8a6d7aeae7326956d747737ccf1575e61e209':
    raise RuntimeError('JUCE source does not match the pinned binary dependency')

# Source is an explicit list; exclude build caches, downloads, Git data and symlinks.
source = []
for name in ['Source', 'Tests', 'Assets', 'cmake', 'scripts', 'packaging', 'docs', 'licenses', '.github']:
    source.extend(p for p in (root/name).rglob('*') if p.is_file() and not p.is_symlink())
source.extend(root/name for name in ['CMakeLists.txt', '.gitignore', 'README.md', 'README.en.md',
                                   'CREDITS.md', 'THIRD_PARTY_NOTICES.md', 'CONTRIBUTING.md', 'LICENSE'])
source = sorted(set(source))
with zipfile.ZipFile(output/f'Pocket-One-{version}-Windows-build-kit.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for item in source:
        archive.write(item, f'Pocket-One-{version}/'+str(item.relative_to(root)))

build_readme = dist/'BUILD-FROM-SOURCE.md'
build_readme.write_text(f'''# Pocket One {version} corresponding source

`Pocket-One/` contains the project; `JUCE/` contains the complete dependency source
at commit 51a8a6d7aeae7326956d747737ccf1575e61e209, including original notices.
No private Git history, compiler/SDK caches or credentials are included.

From this extracted directory, build without fetching JUCE again:

```sh
cmake -S Pocket-One -B build -DJUCE_PATH="$PWD/JUCE" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Windows Developer PowerShell:

```powershell
cmake -S Pocket-One -B build -A x64 -DJUCE_PATH="$PWD/JUCE"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./Pocket-One/packaging/windows/build-installer.ps1 -BuildDir build
```

Windows requires the normal Visual Studio C++ tools/Windows SDK; Linux requires
the development libraries documented in Pocket-One/README.en.md. NSIS 3.11+ is
needed to build a Windows installer. See Pocket-One/LICENSE, CREDITS.md and
THIRD_PARTY_NOTICES.md for licences/credits. Build/test infrastructure is supplied
alongside the source. The Windows cross-build helper is specific to /workspace;
native builds above work independently of the cloud helper layout.
''')
with tarfile.open(output/f'Pocket-One-{version}-corresponding-source.tar.gz', 'w:gz') as archive:
    archive.add(build_readme, f'Pocket-One-{version}/BUILD-FROM-SOURCE.md')
    for item in source:
        archive.add(item, f'Pocket-One-{version}/Pocket-One/'+str(item.relative_to(root)), recursive=False)
    for item in sorted(juce.rglob('*')):
        if item.is_file() and '.git' not in item.relative_to(juce).parts:
            archive.add(item, f'Pocket-One-{version}/JUCE/'+str(item.relative_to(juce)), recursive=False)

win = root/'build-win-msvc/PocketOne_artefacts/Release'
with zipfile.ZipFile(output/f'Pocket-One-{version}-Windows-x64-portable.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for folder in ['VST3', 'Standalone']:
        for item in sorted((win/folder).rglob('*')):
            if item.is_file() and item.suffix.lower() not in ('.pdb', '.lib', '.exp'):
                archive.write(item, f'Pocket-One-{version}/'+str(item.relative_to(win)))
    for item in source:
        if item.suffix == '.md' or 'licenses' in item.parts or item.name == 'LICENSE':
            archive.write(item, f'Pocket-One-{version}/'+str(item.relative_to(root)))
with tarfile.open(output/f'Pocket-One-{version}-linux-x86_64.tar.gz', 'w:gz') as archive:
    native = root/'build/PocketOne_artefacts/Release'
    for item, name in [(native/'VST3/Pocket One.vst3', 'Pocket One.vst3'),
                       (native/'Standalone/Pocket One', 'Pocket One'),
                       (root/'docs', 'docs'), (root/'licenses', 'licenses')]:
        archive.add(item, f'Pocket-One-{version}/'+name)
    for name in ['README.md', 'README.en.md', 'CREDITS.md', 'THIRD_PARTY_NOTICES.md', 'LICENSE']:
        archive.add(root/name, f'Pocket-One-{version}/'+name)
(output/f'Pocket-One-{version}-Windows-x64-Setup.exe').write_bytes(required[0].read_bytes())
with zipfile.ZipFile(output/f'Pocket-One-{version}-Factory-Samples.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for item in sorted((root/'Assets/Factory').iterdir()):
        if item.is_file(): archive.write(item, 'Factory Samples/'+item.name)
    archive.write(root/'licenses/CC0-1.0.txt', 'Factory Samples/CC0-1.0.txt')
    archive.write(root/'docs/sound-bank.md', 'Factory Samples/sound-bank.md')
lines = []
for item in sorted(output.iterdir()):
    if item.is_file() and item.suffix in ('.exe', '.zip', '.gz'):
        item.chmod(0o644)
        lines.append(hashlib.sha256(item.read_bytes()).hexdigest()+'  '+item.name)
(output/'SHA256SUMS.txt').write_text('\n'.join(lines)+'\n')
print(json.dumps({'output': str(output), 'assets': len(lines),
                  'bytes': sum(p.stat().st_size for p in output.iterdir() if p.is_file())}))
