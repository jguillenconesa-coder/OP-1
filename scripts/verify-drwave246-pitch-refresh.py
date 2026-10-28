#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast the complete VoiceCtl pitch refresh with generated tuning, six ADSR
states and Dr. Wave render. Auxiliary Core modules are explicitly absent; the
register-to-render-buffer copy remains own held-pitch scheduling.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
RECORD,RESULT,EVENT=12284,12168,2980
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def inputs(base):
    prior=module('tuning_table',ROOT/'scripts/verify-voice246-tuning-table.py');old,expanded=prior.activation_inputs(base)
    seed=0x52465253
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    events=bytearray();records=bytearray()
    for case in range(512):
        row=bytearray(old[case*240:(case+1)*240]);record=bytearray(expanded[case*9544:(case+1)*9544])
        selected=struct.unpack_from('<I',row,16)[0]
        mode=((case%16)<<12)|(rnd()&4095)
        engine_mode=(((case+5)%16)<<12)|(mode&4095)
        struct.pack_into('<H',row,20+selected*16+8,engine_mode)
        struct.pack_into('<H',record,9324+selected*16+8,engine_mode)
        random=rnd();envelopes=[rnd() for _ in range(40)];entries=[]
        for i in range(128):
            note=(rnd()>>16)%128;velocity=rnd();opaque=rnd()
            explicit=(0,1,0x7fffffff,0x80000000,0xffffffff,rnd())[i%6]
            voice=(i+case)%8-2
            if case%16!=0 and i==case%129-1 and explicit and case%3==0:note=0xffffffff
            if case==128:note,explicit,voice=i,0,i%6
            entries.extend([note,velocity,opaque,explicit,voice&0xffffffff])
        list_image=[rnd(),*entries,case%129,rnd()]
        tail=struct.pack('<685I',mode,random,*envelopes,*list_image)
        events+=row+tail;records+=record+tail
    if len(events)!=512*EVENT or len(records)!=512*RECORD:raise RuntimeError('Invalid refresh dimensions')
    return events,records
def template():
    prior=module('tuning_table',ROOT/'scripts/verify-voice246-tuning-table.py');text=prior.activation_template()
    reload='''P0 = 9552;
P1 = P4 + P0;
imm32 P2, 0xff8000cc
P3 = 40;
LSETUP(adsr_in,adsr_in_end) LC0 = P3;
adsr_in: R0 = [P1++];
adsr_in_end: [P2++] = R0;
P0 = 9548;
P1 = P4 + P0;
R0 = [P1];
imm32 P2, 0xff802bc8
[P2++] = R0;
imm32 R0, 0x31415926
[P2] = R0;
'''
    if text.count('no_reload:')!=1:raise RuntimeError('Unexpected reload boundary')
    text=text.replace('no_reload:',reload+'no_reload:',1)
    old='''P0 = 9452;
P1 = P4 + P0;
R0 = P1;
R1 = [P4 + 4];
imm32 P0, 0x01a84414
CALL (P0);'''
    if text.count(old)!=1:raise RuntimeError('Unexpected prior activation call')
    text=text.replace(old,'''P0 = 9712;
P1 = P4 + P0;
imm32 P2, 0x01dfe730
P3 = 643;
LSETUP(list_in,list_in_end) LC0 = P3;
list_in: R0 = [P1++];
list_in_end: [P2++] = R0;
// Common sound parameters are a separate block from Core engine controls.
P0 = 9324;
P1 = P4 + P0;
imm32 P2, 0x01b60ef0
P3 = 32;
LSETUP(common_in,common_in_end) LC0 = P3;
common_in: R0 = [P1++];
common_in_end: [P2++] = R0;
// Selected common mode deliberately differs from the Core mode word.
P0 = 9544;
P1 = P4 + P0;
R0 = [P1];
P0 = 9320;
P1 = P4 + P0;
R2 = [P1];
R2 <<= 4;
P2 = R2;
imm32 P3, 0x01b60ef8
P2 = P2 + P3;
W[P2] = R0.L;
// Explicitly absent auxiliary Core modules; initialized common ADSR pool.
imm32 P1, 0x01aaad6c
P3 = 6;
R0 = 0;
LSETUP(auxiliary_zero,auxiliary_zero_end) LC0 = P3;
auxiliary_zero: [P1++] = R0;
auxiliary_zero_end: NOP;
imm32 P1, 0x01b6053c
R0 = 1;
[P1] = R0;
imm32 P0, 0x01a854cc
CALL (P0);''')
    old='P5 = P2;\nP0 = 9544;'
    if text.count(old)!=1:raise RuntimeError('Unexpected result boundary')
    text=text.replace(old,'''imm32 P1, 0xff8000cc
P3 = 40;
LSETUP(adsr_out,adsr_out_end) LC0 = P3;
adsr_out: R0 = [P1++];
adsr_out_end: [P2++] = R0;
imm32 P1, 0xff802bc8
R0 = [P1++];
[P2++] = R0;
R0 = [P1];
[P2++] = R0;
imm32 P1, 0x01dfe730
P3 = 643;
LSETUP(list_out,list_out_end) LC0 = P3;
list_out: R0 = [P1++];
list_out_end: [P2++] = R0;
imm32 P1, 0x01aaad6c
P3 = 6;
LSETUP(auxiliary_out,auxiliary_out_end) LC0 = P3;
auxiliary_out: R0 = [P1++];
auxiliary_out_end: [P2++] = R0;
imm32 P1, 0x01b6053c
R0 = [P1];
[P2++] = R0;
imm32 P1, 0x01b60ef0
P3 = 32;
LSETUP(common_out,common_out_end) LC0 = P3;
common_out: R0 = [P1++];
common_out_end: [P2++] = R0;
P5 = P2;
P0 = 12284;''')
    # The expanded own record body exceeds the conditional short-jump range.
    old='IF !CC JUMP record_loop;'
    if text.count(old)!=1:raise RuntimeError('Unexpected outer-loop branch')
    return text.replace(old,'IF CC JUMP records_done;\nJUMP.L record_loop;\nrecords_done:',1)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    raw=a.ldr.read_bytes();mapper=module('ldr_map',ROOT/'scripts/map-op1-ldr.py');meta,_=mapper.inspect(raw)
    start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    base=(ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin').read_bytes()
    if len(base)!=512*9316:raise ValueError('Incomplete base render fixture')
    events,expanded=inputs(base);out.mkdir(parents=True)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(events)
    (out/'probe.s').write_text(template().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(512*RESULT)))
    def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([a.linker,'-T',ROOT/'research/blackfin/voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=512+327680+512*RESULT+9600 or output[512:512+163840]!=output[-163840:]:raise RuntimeError('Incomplete refresh output or changed base banks')
    expected_table=(ROOT/'Tests/ReferenceVectors/voice246-tuning-table-output.bin').read_bytes()[4:9604]
    if output[-163840-9600:-163840]!=expected_table:raise RuntimeError('Generated tuning table changed')
    artifacts={}
    for name,path in [('drwave246-pitch-refresh-overrides.bin',out/'overrides.bin'),('drwave246-pitch-refresh-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Refresh oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    modes=set();lengths=set();lookups=set()
    coverage={'empty_lists':0,'disabled_mode_records':0,'poly_records':0,'single_voice_records':0,'six_voice_records':0,
              'accepted_core_events':0,'rejected_core_events':0,'adsr_note_callbacks':0,'rng_advances':0,'poly_gain_writes':0}
    for case in range(512):
        row=case*EVENT;mode=struct.unpack_from('<h',events,row+240)[0]//4096;count=struct.unpack_from('<I',events,row+2972)[0]
        modes.add(mode);lengths.add(count);loaded,selected=struct.unpack_from('<2I',events,row+12)
        coverage['empty_lists']+=int(count==0);coverage['disabled_mode_records']+=int(mode==4)
        if count==0 or mode==4:continue
        coverage['poly_records' if mode==0 else 'six_voice_records' if mode==3 else 'single_voice_records']+=1
        selected_notes=[]
        if mode==0:
            for i in range(count):
                note=struct.unpack_from('<4Ii',events,row+412+i*20)
                if note[4]>=0:selected_notes.append(note)
            coverage['poly_gain_writes']+=len(selected_notes)
        else:
            note=struct.unpack_from('<4Ii',events,row+412+(count-1)*20)
            selected_notes=[note]*(6 if mode==3 else 1)
        coverage['adsr_note_callbacks']+=len(selected_notes)
        coverage['accepted_core_events' if loaded==selected else 'rejected_core_events']+=len(selected_notes)
        if loaded==selected:
            if mode!=0:coverage['rng_advances']+=len(selected_notes)
            lookups.update(n[0] for n in selected_notes if n[3]==0)
    if modes!=set(range(-8,8)) or lengths!=set(range(129)) or lookups!=set(range(128)):raise RuntimeError('Incomplete refresh mode/list/note coverage')
    report={'ldr_sha256':expected,'refresh':'0x01a854cc','end_exclusive':'0x01a856bc',
        'routine_sha256':hashlib.sha256(image[0x01a854cc-start:0x01a856bc-start]).hexdigest(),
        'records':512,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,'audio_samples':61568,
        'coverage':dict(coverage,signed_mode_nibbles=len(modes),list_lengths=len(lengths),accepted_lookup_notes=len(lookups)),
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
        'limitations':['Complete refresh with explicitly absent auxiliary Core modules and explicitly initialized six-object ADSR pool; not the full instrument graph.',
            'Common and Core sound parameters are explicit inputs, with deliberately different selected-mode words to verify the common-mode source.',
            'ADSR note callbacks and a gain write are exercised, not ADSR rendering or the L1 handoff.',
            'Lists, controls, shared RNG initialization and render ordering remain harness inputs. The allocator and cadence are not recovered.',
            'Generated tuning table is original; register pitch is still held in the render buffer by own harness code.',
            'GNU BF524 deferred D-register model, not hardware/DAW/ASIO timing. Original bytes and ELF remain private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: complete VoiceCtl refresh, ADSR/register/RNG state and Dr. Wave render')
if __name__=='__main__':main()
