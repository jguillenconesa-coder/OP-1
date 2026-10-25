#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Build a local Pocket One bank from user-owned WAV/AIFF files; never uploads audio."""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import wave
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
assets, presets, sources, duplicates = {}, [], [], []
seen, by_name = {}, {}

def aiff_info(data):
    if data[:4] != b'FORM' or data[8:12] not in (b'AIFF', b'AIFC'):
        raise ValueError('Not an AIFF container')
    offset, info, metadata = 12, {}, {}
    while offset + 8 <= len(data):
        tag, size = data[offset:offset+4], int.from_bytes(data[offset+4:offset+8], 'big')
        chunk = data[offset+8:offset+8+size]
        if len(chunk) != size:
            raise ValueError('Truncated AIFF chunk')
        if tag == b'COMM':
            channels, frames, bits = struct.unpack('>hIh', chunk[:8])
            exponent = int.from_bytes(chunk[8:10], 'big') & 0x7fff
            rate = int.from_bytes(chunk[10:18], 'big') * 2.0**(exponent-16383-63)
            info = dict(channels=channels, frames=frames, bits=bits, rate=rate)
        elif tag == b'APPL' and chunk[:4] == b'op-1':
            metadata = json.loads(chunk[4:].rstrip(b' \x00\r\n'))
        offset += 8 + size + size % 2
    if not info: raise ValueError('AIFF COMM chunk missing')
    return info, metadata

def root_note(name, metadata):
    if metadata.get('base_freq', 0) > 0:
        return max(0, min(127, 69 + 12*math.log2(metadata['base_freq']/440)))
    match = re.search(r'(?:^| )([A-G])([#b]?)([0-8])(?:$| )', name)
    if not match: return 60
    letter, accidental, octave = match.groups()
    return 12*(int(octave)+1)+{'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}[letter]+(1 if accidental=='#' else -1 if accidental=='b' else 0)

def recipe(name, root, space=False):
    lower=name.lower()
    keys=any(word in lower for word in ('piano','drags','skog','pluck','bass'))
    pad=not keys and not name.isupper()
    return dict(tone=0, color=1, envelope=.3, attack=.006 if keys else .14 if pad else 0,
                decay=.3, sustain=.85 if pad else .65 if keys else 1,
                release=.42 if pad else .24 if keys else .04,
                sampleRoot=root, sampleLoop=0, effect=.36 if space else 0,
                delayTime=.375, delayFeedback=.48 if space else .25,
                lfoRate=.25, lfoDepth=.025 if space and pad else 0,
                lfoDestination=1, lfoShape=0, drive=0)

for path in sorted(args.input.rglob('*')):
    if not path.is_file() or path.suffix.lower() not in ('.wav','.aif','.aiff'): continue
    data=path.read_bytes(); digest=hashlib.sha256(data).hexdigest()
    if digest in seen:
        duplicates.append(dict(name=path.name, duplicateOf=seen[digest], sha256=digest));continue
    seen[digest]=path.name
    metadata={};parts=[]
    if path.suffix.lower()=='.wav':
        with wave.open(io.BytesIO(data)) as reader:
            rate=reader.getframerate();frames=reader.getnframes()
            info=dict(rate=rate,frames=frames,channels=reader.getnchannels(),bits=reader.getsampwidth()*8)
            if frames>rate*16:
                for start in range(0,frames,rate*16):
                    audio=reader.readframes(min(rate*16,frames-start));out=io.BytesIO()
                    with wave.open(out,'wb') as writer:
                        writer.setparams((reader.getnchannels(),reader.getsampwidth(),rate,0,'NONE','not compressed'));writer.writeframes(audio)
                    parts.append((f'{path.stem.strip()} - Part {len(parts)+1}',out.getvalue(),start/rate))
            else: parts=[(path.stem.strip(),data,0)]
    else:
        info,metadata=aiff_info(data)
        if info['frames']/info['rate']>16: raise ValueError(f'AIFF exceeds sampler duration: {path.name}')
        if metadata.get('type') not in (None,'sampler'): raise ValueError(f'Not a sampled instrument: {path.name}')
        parts=[(path.stem.strip(),data,0)]
    sources.append(dict(name=path.name,sha256=digest,seconds=info['frames']/info['rate'],audio=info,op1Metadata=metadata))
    for name,content,start in parts:
        key='samples/'+hashlib.sha256(content).hexdigest()[:24]+path.suffix.lower()
        assets[key]=content;by_name.setdefault(path.stem.strip(),key)
        note=root_note(path.stem,metadata)
        presets.append(dict(name=name+' / Dry',sample=key,parameters=recipe(name,note),source=path.name,startSeconds=start))
        if path.suffix.lower() in ('.aif','.aiff'):
            presets.append(dict(name=name+' / Space',sample=key,parameters=recipe(name,note,True),source=path.name,startSeconds=start))

for name,hits in [
    ('Wood and insects', ['TREEKICK','KUBB 1','BLADSHAKER','MYGG','KUBB 2','FYLLESNUBBEL','FLINTASTEK','BI']),
    ('Midsummer voices', ['KUBBKICK','SNAPSDAGS','BLADSHAKER','HUSFLUGA','KUBB 2','NU TAR VI DEN','BIRAN','DYK I VIK'])]:
    if all(hit in by_name for hit in hits):
        presets.append(dict(name='KIT / '+name,kit=[by_name[hit] for hit in hits],parameters=dict(tone=.5,color=1,effect=.05,attack=0,release=.1,drive=0),slots=hits))
manifest=dict(formatVersion=1,name='User sample collection',presets=presets,
              provenance='User-supplied audio. Original authors/license not yet identified; not relicensed as CC0 or AGPL.',
              limitations='Sampler playback is mono (stereo is averaged). Original OP-1 engine/effect parameters are not imported. Dry/Space are Pocket One recipes. Long WAVs are split into <=16-second parts.',
              sources=sources,duplicatesRemoved=duplicates)
args.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('bank.json',json.dumps(manifest,ensure_ascii=False,indent=2))
    archive.writestr('README.txt','Pocket One 0.2.2+: click BANK, choose this ZIP, then select USER sounds.\nImport while transport is stopped. The bank is stored locally; sessions embed their selected audio.\nOriginal sample authors/license remain to be identified. This is not an official Teenage Engineering sound bank.\n'+manifest['limitations']+'\n')
    for name,data in assets.items():archive.writestr(name,data)
print(json.dumps(dict(bank=str(args.output),presets=len(presets),unique_sources=len(sources),audio_entries=len(assets),duplicates_removed=len(duplicates),bytes=args.output.stat().st_size)))
