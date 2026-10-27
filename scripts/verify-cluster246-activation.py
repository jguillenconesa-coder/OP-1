#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce VoiceCtl preparation and Cluster activation with explicit tuning.
Original instructions/ELF and the bounded original note-table copy stay private.
The following held-increment render is an explicit harness schedule, not Core's
original pitch provider or full ADSR/output route.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def note_header(words):
    rows=['    '+','.join(str(x) for x in words[i:i+8])+',' for i in range(0,128,8)]
    return '// SPDX-License-Identifier: AGPL-3.0-only\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace pocket::reference {\n// Numeric observations of the rev246 128-note table copied by original code.\n// Tuning and subsequent per-sample pitch preparation are separate dependencies.\ninline constexpr std::array<int32_t,128> voice246NotePitch{{\n'+'\n'.join(rows)+'\n}};\n}\n'
def inputs():
    c=json.loads((ROOT/'research/blackfin/cluster246-contract.json').read_text());norm=[0]*8
    for row in c['normalization']['observed_entries']:norm[row['count']-1]=row['q15']
    data=bytearray(struct.pack('<8h32i',*norm,*c['damping']['verified_q31'],*[0]*8));seed=0x41435456
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    for case in range(1024):
        independent=case<512;block=(case-512)%64
        action=(1 if independent or block==0 else 0)|(2 if case%7==0 or (not independent and block==0) else 0)
        voice=case%6;loaded=case//6%8
        selected=(loaded+1)%8 if case>=256 and case%3==0 else loaded
        note=case//2%128;override=0 if case%2==0 else (rnd() or 1)
        velocity=(rnd()&0xffff0000)|(0,1,127,128,255,32767,32768,65535)[case//2%8]
        tuning=0x40000000 if case<256 else (0,1,0x40000000,0x7fffffff,0x80000000,0xffffffff,rnd())[case%7]
        pool=bytearray(struct.pack('<162I',*[rnd() for _ in range(162)]))
        for slot in range(6):struct.pack_into('<I',pool,slot*108,0x0190f67c);pool[slot*108+106]=rnd()%2
        context=struct.pack('<8I',*[rnd() for _ in range(8)]);rng=rnd()
        registers=struct.pack('<16I',*[rnd() for _ in range(16)]);sounds=[]
        for sound in range(8):
            count=(1,4,5,6,7)[(case+sound)%5]
            sounds.extend([count*2048+1024,rnd()%32768,512+rnd()%23553,3+rnd()%1636,*[rnd()%65536-32768 for _ in range(4)]])
        data+=struct.pack('<9I',action,voice,loaded,selected,note,velocity,rnd(),override,tuning)+pool+context+struct.pack('<I',rng)+registers+struct.pack('<64h',*sounds)
    if len(data)!=144+1024*912:raise RuntimeError('Invalid activation input dimensions')
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0;l1=[]
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if address<=0xffa00a78<address+n:l1.append({'header_offset':hex(cursor),'address':hex(address),'bytes':n,'flags':hex(flags)})
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    # Relocate the original loop unchanged; its local offsets remain relative.
    # It contains no external calls. Only our RTS is appended by the template.
    (out/'note-copy.bin').write_bytes(image[0x01a86700-base:0x01a8672e-base])
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'cluster-activation.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(1024*1020)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=120,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=512+1024*1020:raise RuntimeError('Incomplete activation output')
    if output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Original note-table copy differs from its source')
    header=ROOT/'Source/Voice246NoteTable.h';source=note_header(struct.unpack('<128i',output[:512]))
    if a.record:header.write_text(source)
    if header.read_text()!=source:raise RuntimeError('Numeric note table differs')
    artifacts={}
    for name,path in [('cluster246-activation-input.bin',out/'inputs.bin'),('cluster246-activation-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    evidence={'ldr_sha256':expected,'voice_activation':'0x01a84414','core_note_on':'0x01967854','note_table_source':'0x018c21b0','note_table_destination':'0x01dfe530','bounded_copy_fragment':{'start':'0x01a86700','end_exclusive':'0x01a8672e','sha256':sha(out/'note-copy.bin'),'scope':'Unmodified PC-relative copy loop, isolated with own appended RTS; not the whole initializer'},'tuning_index_address':'0x018c2184','observed_tuning_index':struct.unpack_from('<I',image,0x018c2184-base)[0],'tuning_word_address':'0x027fd9e0','tuning_contract':'One explicit caller-supplied signed32 word at observed index 1200. Original/default tuning coefficient not recovered.','independent_cases':512,'continuous_streams':8,'blocks_per_stream':64,'cases':1024,'samples':131072,'held_increment_schedule':'Harness copies each prepared pitch word into all 128 phase-increment positions; not the original pitch provider.','l1_handoff_blocker':{'covering_blocks':l1,'candidate_references':['0xffa00a78','0xffa00b7a','0xffa00b82'],'scope':'Readable address materializations/call adjacent to ciphertext. No validated whole control/data flow or execution of L1 consumer claimed.'},'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation','limitations':['Activation is specialized to Cluster with explicit tuning and known normalization counts 1,4,5,6,7. Counts 2,3,8 remain unknown.','The original activation function has no defined result; C++ bool only reports adapter acceptance.','No complete ADSR pending-field consumer, pitch provider, scheduler, allocator, mixer, plugin integration or new presets claimed.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: 128 original note-table words, 1024 VoiceCtl/Cluster activation records and 131072 explicitly scheduled samples reproduced')
if __name__=='__main__':main()
