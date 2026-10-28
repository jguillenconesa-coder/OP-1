#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast the original VoiceCtl bend decoder, not the MIDI dispatcher/render.
Only numeric fixtures are recordable. Original image, instructions and ELF stay
in a new private output directory outside the checkout.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES=65536+256
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    records=bytearray();seed=0x42454e44
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    for case in range(CASES):
        raw=case&65535
        # Every unsigned byte pair occurs independently, including non-MIDI
        # high bits. A separate 256-call stream retains the control image.
        lo,hi=raw&255,raw>>8
        if case>=65536:lo,hi=((0,0),(127,63),(0,64),(127,127),(255,255),(255,63),(0,128),(1,64))[case%8]
        message=bytes([(case*37)&255,lo,hi,(case*13)&255])
        control=struct.pack('<4I',*[rnd() for _ in range(4)])
        records+=struct.pack('<I',int(case<=65536))+message+control+struct.pack('<2I',rnd(),rnd())
    return records
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    meta,image=mapper.inspect(a.ldr.read_bytes());start=int(meta['sdram_start'],16)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    text=(ROOT/'research/blackfin/voice-pitch-bend.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*72))
    (out/'probe.s').write_text(text)
    def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([a.linker,'-T',ROOT/'research/blackfin/voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--watch-cycles-int','90000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=CASES*72:raise RuntimeError('Incomplete original decoder output')
    artifacts={}
    for name,path in [('voice246-pitch-bend-input.bin',out/'inputs.bin'),('voice246-pitch-bend-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original bend oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'routine':'0x01a84efa','routine_end_exclusive':'0x01a84f8c',
        'routine_sha256':hashlib.sha256(image[0x01a84efa-start:0x01a84f8c-start]).hexdigest(),
        'records':CASES,'independent_unsigned_byte_pairs':65536,'valid_midi_pairs':16384,
        'continuous_calls':256,'control_image_bytes':16,'source_image_bytes':32,'pitch_register_words':6,
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
        'limitations':['Complete decoder only; source packet has exactly three readable message bytes. Invalid-length original exception paths are not executed.',
            'Status byte is an explicit ignored input to this decoder; original dispatcher, channel filtering and Core gating are not reconstructed.',
            'No pitch coefficient, render-buffer writer, Dr. Wave integration, audio samples or original bend-to-frequency mapping established.',
            'GNU BF524 deferred D-register model, not hardware/latency validation; no firmware, instructions or ELF distributed.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: original bend decoder, 65536 independent byte pairs and 256 continuous calls')
if __name__=='__main__':main()
