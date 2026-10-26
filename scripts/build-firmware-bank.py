#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Build a LOCAL reference sample bank from an operator-supplied OP-1 firmware.
No upload, device update, native target execution, or original DSP conversion.
Raw interpretation: signed PCM16 little endian, mono, explicit caller rate.
"""
import argparse
from contextlib import closing
import hashlib
import io
import json
import lzma
import math
from pathlib import Path, PurePosixPath
import sqlite3
import struct
import tarfile
import tempfile
import wave
import zipfile
import zlib
LIMIT=128*1024*1024

def read_members(path):
    if not 17<=path.stat().st_size<=LIMIT:raise ValueError('Input size outside limit')
    data=path.read_bytes()
    if struct.unpack('<I',data[:4])[0]!=zlib.crc32(data[4:]):raise ValueError('CRC mismatch')
    decoder=lzma.LZMADecompressor(format=lzma.FORMAT_ALONE,memlimit=LIMIT)
    unpacked=decoder.decompress(data[4:],max_length=LIMIT+1)
    if not decoder.eof or decoder.unused_data or len(unpacked)>LIMIT:raise ValueError('Invalid/oversized compression')
    members={};total=0
    with tarfile.open(fileobj=io.BytesIO(unpacked)) as archive:
        for i,member in enumerate(archive):
            name=PurePosixPath(member.name)
            if i>=10000 or name.is_absolute() or '..' in name.parts or '\\' in member.name:raise ValueError('Unsafe archive')
            if member.isdir():continue
            if not member.isfile():raise ValueError('Unsupported special member')
            total+=member.size
            if total>LIMIT or member.size<0:raise ValueError('Archive exceeds limit')
            if member.name=='content/op1_factory.db' or member.name.startswith(('content/audio/factory_synth/','content/audio/factory_drum/')):
                if member.name in members:raise ValueError('Duplicate member')
                members[member.name]=archive.extractfile(member).read()
    return members,hashlib.sha256(data).hexdigest()

def wav_bytes(raw,rate):
    if not raw or len(raw)%2:raise ValueError('Invalid PCM16 length')
    stream=io.BytesIO()
    with wave.open(stream,'wb') as writer:
        writer.setparams((1,2,rate,0,'NONE','not compressed'));writer.writeframes(raw)
    return stream.getvalue()

def drum_region(start,end,frames,rate):
    # Q31 positions on a 12-second coordinate. End is inclusive.
    if not 0<=start<=end<2**31:raise ValueError('Invalid Q31 drum bounds')
    scale=rate*12/(2**31)
    first=round(start*scale);last=round(end*scale)+1
    if not 0<=first<last<=frames:raise ValueError('Drum region outside source audio')
    return first,last

def build(source,output,rate):
    if rate!=44100:raise ValueError('Only the explicitly selected 44100 Hz reference profile is supported')
    members,digest=read_members(source);assets={};presets=[];regions=[];unmapped=[]
    with tempfile.TemporaryDirectory(prefix='pocket-factory-reference-') as directory:
        database=Path(directory)/'factory.db';database.write_bytes(members['content/op1_factory.db'])
        with closing(sqlite3.connect(database.as_uri()+'?mode=ro',uri=True)) as conn:
            conn.execute('PRAGMA trusted_schema=OFF');conn.execute('PRAGMA query_only=ON');conn.setlimit(sqlite3.SQLITE_LIMIT_LENGTH,8000000)
            for table,kind in [('synth_presets','sampler'),('drum_presets','drum')]:
                for patch, in conn.execute('SELECT patch FROM '+table+' LIMIT 1024'):
                    patch=json.loads(patch)
                    if patch['type']!=kind:continue
                    name=patch['name'];group='factory_synth' if kind=='sampler' else 'factory_drum'
                    path=f'content/audio/{group}/{name}.raw'
                    if path not in members:raise ValueError('Missing sample '+path)
                    raw=members[path];frames=len(raw)//2
                    def add(content):
                        key='samples/'+hashlib.sha256(content).hexdigest()[:24]+'.wav';assets[key]=wav_bytes(content,rate);return key
                    params=dict(tone=0 if kind=='sampler' else .5,color=1,effect=0,drive=0,attack=0,decay=.25,sustain=1,release=.1,lfoDepth=0,sampleLoop=0,sampleClean=1)
                    preset=dict(name=name+' / RAW reference',source=path,parameters=params)
                    if kind=='sampler':
                        frequency=float(patch.get('base_freq',0))
                        if not math.isfinite(frequency) or frequency<=0:raise ValueError('Invalid sample root')
                        params['sampleRoot']=max(0,min(127,69+12*math.log2(frequency/440)))
                        preset['sample']=add(raw)
                    else:
                        if len(patch['start'])!=24 or len(patch['end'])!=24:raise ValueError('Expected 24 drum regions')
                        preset['kit']=[]
                        for pad,(start,end) in enumerate(zip(patch['start'],patch['end'])):
                            first,last=drum_region(start,end,frames,rate);preset['kit'].append(add(raw[first*2:last*2]));regions.append(dict(kit=name,pad=pad+1,start_frame=first,end_exclusive=last,frames=last-first))
                        # Preserve unknown original values for provenance, never relabel as mapped DSP.
                        unmapped.append(dict(name=name,fields=['pitch','volume','reverse','playmode','fx_params','lfo_params','dyna_env']))
                    presets.append(preset)
    manifest=dict(formatVersion=1,name='OP-1 firmware reference (local)',presets=presets,
                  provenance=dict(input_sha256=digest,source='Operator-supplied firmware; original rights retained; local reference only',raw_profile='PCM16 LE mono / 44100 Hz (inferred)',drum_positions='Q31 / 12 seconds, inclusive end',
                                  limitations=['Original FX, envelopes, LFO and synthesis engines not reconstructed.','Sampler trims and per-pad pitch/gain/playmode are not mapped; these are dry source references.','Not an exact original preset importer or verified audio-equivalent instrument.'],unmapped=unmapped))
    with output.open('xb') as stream:
        with zipfile.ZipFile(stream,'w',zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('bank.json',json.dumps(manifest,indent=2));archive.writestr('regions.json',json.dumps(regions,indent=2))
            for path,data in assets.items():archive.writestr(path,data)
    return dict(presets=len(presets),kits=sum('kit' in p for p in presets),samplers=sum('sample' in p for p in presets),regions=len(regions),audio_files=len(assets),bytes=output.stat().st_size)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--firmware',required=True,type=Path);parser.add_argument('--output',required=True,type=Path);parser.add_argument('--raw-rate',required=True,type=int);args=parser.parse_args();print(json.dumps(build(args.firmware,args.output,args.raw_rate)))
if __name__=='__main__':main()
