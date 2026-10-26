#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Build an additional experimental SAMPLER bank from independently rendered Pd cores.
Raw knob words come from operator-supplied revision 246 firmware. Their complete
original control route is unverified; these are not full original presets.
"""
import argparse
from array import array
from contextlib import closing
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import wave
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def build(firmware,renderer,output,seconds=6):
    metadata=json.loads((ROOT/'research/blackfin/phase-controls.json').read_text())
    if not 1<=seconds<=12:raise ValueError('Duration must be between one and 12 seconds')
    if not renderer.is_file():raise ValueError('Build the native pd_reference_render tool first')
    spec=importlib.util.spec_from_file_location('bank_reader',ROOT/'scripts/build-firmware-bank.py')
    reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(reader)
    members,digest=reader.read_members(firmware)
    if digest!=metadata['firmware_sha256']:raise ValueError('This study profile requires the inventoried revision 246 firmware')
    database_bytes=members['content/op1_factory.db']
    if hashlib.sha256(database_bytes).hexdigest()!=metadata['database_sha256']:raise ValueError('Factory database does not match the research profile')
    assets={};presets=[];rate=44100;root_note=60;frames=rate*seconds
    with tempfile.TemporaryDirectory(prefix='pocket-pd-study-') as folder:
        temporary=Path(folder);database=temporary/'factory.db';database.write_bytes(database_bytes)
        with closing(sqlite3.connect(database.as_uri()+'?mode=ro',uri=True)) as connection:
            connection.execute('PRAGMA trusted_schema=OFF');connection.execute('PRAGMA query_only=ON')
            connection.setlimit(sqlite3.SQLITE_LIMIT_LENGTH,8000000)
            patches=[json.loads(row[0]) for row in connection.execute('SELECT patch FROM synth_presets LIMIT 1024')]
        patches=sorted((p for p in patches if p['type']=='phase'),key=lambda p:p['name'])
        if len(patches)!=len(metadata['presets']):raise ValueError('Unexpected Phase preset count')
        for index,(patch,known) in enumerate(zip(patches,metadata['presets'])):
            if patch['name']!=known['name'] or patch['knobs'][:4]!=known['controls']:raise ValueError('Unexpected Phase control data')
            path=temporary/f'core-{index}.pcm'
            command=[str(renderer.resolve()),str(path),str(rate),str(root_note),str(frames),*[str(x) for x in known['controls']]]
            subprocess.run(command,check=True,timeout=30,stdout=subprocess.DEVNULL)
            data=path.read_bytes()
            if len(data)!=frames*2:raise ValueError('Renderer returned an incorrect PCM length')
            samples=array('h');samples.frombytes(data)
            if sys.byteorder!='little':samples.byteswap()
            # End one-shot samples cleanly; preserve the initial kernel output.
            tail=rate//100
            for i in range(tail):samples[-tail+i]=round(samples[-tail+i]*(tail-1-i)/(tail-1))
            if not any(samples):raise ValueError('Unexpected silent Pd core: '+patch['name'])
            if sys.byteorder!='little':samples.byteswap()
            pcm=samples.tobytes();asset='samples/'+hashlib.sha256(pcm).hexdigest()[:24]+'.wav'
            if asset not in assets:
                buffer=io.BytesIO()
                with wave.open(buffer,'wb') as writer:writer.setparams((1,2,rate,0,'NONE','not compressed'));writer.writeframes(pcm)
                assets[asset]=buffer.getvalue()
            presets.append(dict(name=patch['name']+' / Core study',sample=asset,source='Independent Pd core / stored words: '+patch['name'],
                parameters=dict(tone=0,color=1,envelope=.25,attack=0,decay=.25,sustain=1,release=.12,effect=0,drive=0,lfoDepth=0,sampleRoot=root_note,sampleLoop=0,sampleClean=1),
                research=known))
    limitations=['Experimental oscillator-core renders, played by the Pocket One SAMPLER; not native Phase presets or recordings from an OP-1.',
        'Raw stored control words are fed directly to the isolated Pd reconstruction; their original routing/scaling is not established.',
        'Original ADSR, FX, LFO, octave behavior, original frequency preparation and full voice mixing are not applied.',
        'Equal raw control sets intentionally share one audio asset; original presets with different FX/envelopes can sound alike here.',
        'Host NCO at MIDI 60 / A4=440 Hz / 44100 Hz, source sine table, six-second default one-shots with a 10 ms tail fade.',
        'The source sine matches firmware data; the apparent GNU parallel-copy timing discrepancy remains unresolved against hardware.']
    manifest=dict(formatVersion=1,name='Phase 246 core studies (experimental)',presets=presets,
        provenance=dict(firmware_sha256=digest,database_sha256=metadata['database_sha256'],renderer_sha256=hashlib.sha256(renderer.read_bytes()).hexdigest(),
            raw_format='PCM16 LE mono',rate=rate,root_midi=root_note,frames=frames,tail_fade_frames=rate//100,limitations=limitations))
    manifest['provenance']['limitations'][4]=limitations[4].replace('six-second default',str(seconds)+'-second')
    with output.open('xb') as destination:
        with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as archive:
            # Fixed timestamps make identical builds byte reproducible.
            for name,data in [('bank.json',json.dumps(manifest,indent=2)),('README.txt','PHASE 246 CORE STUDIES — EXPERIMENTAL\n\n'+'\n'.join(limitations))]+sorted(assets.items()):
                archive.writestr(zipfile.ZipInfo(name,date_time=(2026,10,26,0,0,0)),data,compress_type=zipfile.ZIP_DEFLATED)
    return dict(presets=len(presets),unique_audio=len(assets),bytes=output.stat().st_size,frames_per_asset=frames,
                sha256=hashlib.sha256(output.read_bytes()).hexdigest())

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware',type=Path,required=True);parser.add_argument('--renderer',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--seconds',type=int,default=6)
    args=parser.parse_args();print(json.dumps(build(args.firmware,args.renderer,args.output,args.seconds),indent=2))
