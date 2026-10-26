#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Validate the actual offline renderer against continuous original-kernel oracles."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
renderer=Path(sys.argv[1]).resolve()
wrapper=[str(ROOT/'scripts/run-wine.sh')] if os.name!='nt' and renderer.suffix.lower()=='.exe' else []
profile=json.loads((ROOT/'research/blackfin/phase-controls.json').read_text())
oracle=(ROOT/'Tests/ReferenceVectors/pd-stream-output.bin').read_bytes()
with tempfile.TemporaryDirectory(prefix='pocket-pd-render-test-') as folder:
    for preset,patch in enumerate(profile['presets']):
        for index,rate in enumerate((44100,48000,96000)):
            output=Path(folder)/f'{preset}-{rate}.pcm'
            target=('Z:'+output.as_posix()) if wrapper else str(output)
            command=wrapper+[str(renderer),target,str(rate),'60','2048',*[str(x) for x in patch['controls']]]
            subprocess.run(command,check=True,timeout=30,stdout=subprocess.DEVNULL)
            block=(preset*9+index*3)*16
            expected=b''.join(oracle[(block+i)*272:(block+i)*272+256] for i in range(16))
            if output.read_bytes()!=expected:raise AssertionError('Rendered PCM differs from original kernel: '+patch['name'])
    previous=output.read_bytes()
    failure=subprocess.run(command,timeout=30,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if failure.returncode==0 or output.read_bytes()!=previous:raise AssertionError('Renderer overwrote an existing file')
print('PASS: offline renderer matches 98,304 original-kernel samples across 16 raw preset control sets and three rates; overwrite rejected')
