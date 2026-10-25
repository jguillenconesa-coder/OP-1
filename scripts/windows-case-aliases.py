#!/usr/bin/env python3
"""Add SDK filename aliases for Windows' case-insensitive include semantics.
No header contents are changed. xwin already supplies many such aliases.
"""
import pathlib
import re
sdk = pathlib.Path('/workspace/windows-tools/msvc-sdk')
roots = [sdk/'crt/include', *(sdk/'sdk/include').iterdir()]
requests = set()
libraries = set()
for source in pathlib.Path('/workspace/juce-installed/include/JUCE-8.0.6/modules').rglob('*'):
    if source.is_file() and source.suffix in ('.h', '.cpp', '.c', '.mm'):
        text = source.read_text(errors='ignore')
        libraries.update(re.findall(r'"([A-Za-z0-9_.-]+\.lib)"', text))
        requests.update(re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', text, re.M))
count = 0
for root in roots:
    if not root.is_dir():
        continue
    known = {str(path.relative_to(root)).lower(): path for path in root.rglob('*') if path.is_file()}
    for name in requests:
        if '..' in pathlib.PurePosixPath(name).parts:
            continue
        alias = root / name
        target = known.get(name.lower())
        if target and not alias.exists() and alias.parent.is_dir():
            alias.symlink_to(target.resolve())
            count += 1
for root in [sdk/'crt/lib/x86_64', sdk/'sdk/lib/ucrt/x86_64', sdk/'sdk/lib/um/x86_64']:
    known = {p.name.lower(): p for p in root.iterdir() if p.is_file()}
    for name in libraries:
        target = known.get(name.lower())
        if target and not (root/name).exists():
            (root/name).symlink_to(target.resolve())
            count += 1
print(f'Added {count} SDK case aliases; header contents unchanged')
