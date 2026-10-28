#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast VoiceCtl pitch-register updates, then the original Dr. Wave render.
The final register-to-render-buffer copy remains an explicit held-pitch harness.
No original instruction bytes are distributed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES,RECORD,RESULT,BANK=512,12128,11828,163840
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs(base):
    activation=module('drwave_activation',ROOT/'scripts/verify-drwave246-activation.py')
    events,expanded=activation.inputs(base)
    seed=0x50544348
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    records=bytearray();overrides=bytearray()
    for case in range(CASES):
        query=(case%128,0xffffffff,0x80000000)[case%3]
        pitch=(0,1,255,256,0x7fffffff,0x80000000,0xffffffff,rnd())[case%8]
        tuning=(0,1,0x40000000,0x7fffffff,0x80000000,0xffffffff,rnd())[case//8%7]
        if case%32==1:pitch=tuning=0x80000000
        count=case%129
        entries=[]
        for i in range(128):
            voice=(i+case)%8-2
            # Alternate broadcast and selective-only lists. A broadcast does
            # not require the entry's note identity to match the query.
            if voice==-2 and case%2:voice=-1
            note=query if (i+case)%3==0 else (query+1)&0xffffffff
            if voice==-2:note=(query+1)&0xffffffff
            entries.extend([note,rnd(),rnd(),rnd(),voice&0xffffffff])
        tail=struct.pack('<648I',query,tuning,rnd(),pitch,rnd(),rnd(),*entries,count,rnd())
        records+=expanded[case*9536:(case+1)*9536]+tail
        overrides+=events[case*232:(case+1)*232]+tail
    assert len(records)==CASES*RECORD and len(overrides)==CASES*2824
    return overrides,records

def template():
    text=(ROOT/'research/blackfin/drwave-activation.s.in').read_text()
    before='R0 = [P4 + 4];\nP0 = R0;\nimm32 P1, 0xff8012d4'
    insert='''// Own caller: mutable scalar with guards and all 128 list entries.
P0 = 9544;
P1 = P4 + P0;
imm32 P2, 0x02f01000
P3 = 3;
LSETUP(update_scalar_in,update_scalar_in_end) LC0 = P3;
update_scalar_in: R0 = [P1++];
update_scalar_in_end: [P2++] = R0;
imm32 P2, 0x01dfe730
P3 = 643;
LSETUP(update_list_in,update_list_in_end) LC0 = P3;
update_list_in: R0 = [P1++];
update_list_in_end: [P2++] = R0;
P0 = 9536;
P1 = P4 + P0;
R0 = [P1++];
R2 = [P1];
imm32 P2, 0x027fd9e0
[P2] = R2;
imm32 R1, 0x02f01004
imm32 P0, 0x01a85cbe
CALL (P0);
'''
    assert text.count(before)==1
    text=text.replace(before,insert+before)
    after='P5 = P2;\nP0 = 9536;'
    output='''imm32 P1, 0x02f01000
P3 = 3;
LSETUP(update_scalar_out,update_scalar_out_end) LC0 = P3;
update_scalar_out: R0 = [P1++];
update_scalar_out_end: [P2++] = R0;
imm32 P1, 0x01dfe730
P3 = 643;
LSETUP(update_list_out,update_list_out_end) LC0 = P3;
update_list_out: R0 = [P1++];
update_list_out_end: [P2++] = R0;
P5 = P2;
P0 = 12128;'''
    assert text.count(after)==1
    return text.replace(after,output)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');args=p.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    raw=args.ldr.read_bytes();mapper=module('ldr_map',ROOT/'scripts/map-op1-ldr.py');meta,_=mapper.inspect(raw)
    start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    if struct.unpack_from('<I',image,0x018c2184-start)[0]!=1200:raise RuntimeError('Unexpected tuning index')
    basepath=ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin';base=basepath.read_bytes()
    if len(base)!=CASES*9316:raise ValueError('Incomplete source fixture')
    changes,expanded=inputs(base);out.mkdir(parents=True)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    (out/'probe.s').write_text(template().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=180,**kw)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([args.linker,'-T',ROOT/'research/blackfin/voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=512+BANK*2+CASES*RESULT or output[512:512+BANK]!=output[-BANK:]:raise RuntimeError('Incomplete output or changed banks')
    if output[:512]!=image[0x018c21b0-start:0x018c23b0-start]:raise RuntimeError('Note-table copy differs')
    artifacts={}
    for name,path in [('drwave246-pitch-update-overrides.bin',out/'overrides.bin'),('drwave246-pitch-update-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Pitch-update oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    coverage={'empty_lists':0,'broadcast_records':0,'selective_records':0,'updates_after_rejected_activation':0}
    lengths=set();voices=set()
    for case in range(CASES):
        offset=case*2824;u=offset+232
        query=struct.unpack_from('<I',changes,u)[0];count=struct.unpack_from('<I',changes,u+2584)[0]
        lengths.add(count);coverage['empty_lists']+=int(count==0);targets=set();broadcast=False
        for i in range(count):
            note,_,_,_,voice=struct.unpack_from('<4Ii',changes,u+24+i*20)
            if voice==-2:targets.update(range(6));broadcast=True
            elif voice>=0 and note==query:targets.add(voice)
        coverage['broadcast_records']+=int(broadcast)
        coverage['selective_records']+=int(bool(targets) and not broadcast)
        loaded,selected=struct.unpack_from('<2I',changes,offset+12)
        coverage['updates_after_rejected_activation']+=int(bool(targets) and loaded!=selected)
        voices.update(targets)
    if lengths!=set(range(129)) or voices!=set(range(6)) or not all(coverage.values()):raise RuntimeError('Incomplete update coverage')
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'pitch_update':'0x01a85cbe',
        'records':CASES,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,
        'coverage':dict(coverage,list_lengths=len(lengths),voice_slots=len(voices)),
        'artifacts':artifacts,'routine_sha256':hashlib.sha256(image[0x01a85cbe-start:0x01a85d52-start]).hexdigest(),
        'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
        'limitations':['Explicit tuning; no original/default calibration.',
            'Register pitch is held by the harness in the following render buffer, not by a recovered original writer.',
            'List entries, count, events, voice and counter are explicit test inputs; allocator, clock, ADSR, output and plugin integration remain pending.',
            'GNU BF524 with documented deferred D-register patch; not hardware validation.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: 512 original VoiceCtl pitch updates followed by Dr. Wave render')
if __name__=='__main__':main()
