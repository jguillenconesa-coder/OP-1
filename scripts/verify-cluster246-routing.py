#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce Cluster Core callbacks and explicitly scheduled six-voice renders.
Original instructions and ELF stay in a new private directory outside checkout.
This is not the original allocator, audio scheduler, envelope or output mixer.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    contract=json.loads((ROOT/'research/blackfin/cluster246-contract.json').read_text())
    norms=[0]*8
    for row in contract['normalization']['observed_entries']:norms[row['count']-1]=row['q15']
    data=bytearray(struct.pack('<8h32i',*norms,*contract['damping']['verified_q31'],*[0]*8))
    seed=0x524f5554
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    for case in range(1024):
        independent=case<256;block=(case-256)%64
        action=(1 if independent or block==0 else 0)|(2 if case%7==0 or (not independent and block==0) else 0)
        action|=4 if (independent and case%4!=0) or (not independent and block%3==0) else 0
        action|=8 if (independent and case%5==0) or (not independent and block%11==7) else 0
        action|=32 if case%5==0 else 0
        # 12 continuous streams alternate ascending and descending voice order.
        stream=max(0,(case-256)//64)
        voice=case%6 if independent or stream%2==0 else 5-case%6
        loaded=(case//6)%8;selected=loaded if case%3 else (loaded+1)%8
        pool=bytearray(struct.pack('<162I',*[rnd() for _ in range(162)]))
        for slot in range(6):
            struct.pack_into('<I',pool,slot*108,0x0190f67c)
            pool[slot*108+106]=rnd()%2
        context=struct.pack('<8I',*[rnd() for _ in range(8)])
        rng=rnd();sounds=[]
        for sound in range(8):
            count=(1,4,5,6,7)[(case+sound)%5]
            sounds.extend([count*2048+1024,rnd()%32768,512+rnd()%23553,3+rnd()%1636,
                           *[int(rnd()%65536)-32768 for _ in range(4)]])
        # Per-sample pitch is an explicit external input, never recovered note prep.
        pitch=[rnd() if independent and case%8==0 else
               round(55*2**(voice%5)*2**(.01*math.sin((case*128+j)/1000))/88200*2**32)
               for j in range(128)]
        data+=struct.pack('<4I',action,voice,loaded,selected)+pool+context+struct.pack('<I64h128I',rng,*sounds,*pitch)
    if len(data)!=144+1024*1340:raise RuntimeError('Invalid input dimensions')
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'cluster-routing.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(1024*1104)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=120,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=1024*1104:raise RuntimeError('Incomplete routing output')
    artifacts={}
    for name,path in [('cluster246-routing-input.bin',out/'inputs.bin'),('cluster246-routing-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    evidence={'ldr_sha256':expected,'core_note_on':'0x01967854','core_release':'0x019678f0','core_factory':'0x01967960','runtime_cluster_type':4,'producer_pointers':'0x01aaad18','loaded_sound':'0x01aaac0c','selected_sound':'0xff8016d4','sound_controls':'0xff8013c4','render':'0x01960288','independent_cases':256,'continuous_streams':12,'blocks_per_stream':64,'rendered_blocks':1024,'samples':131072,'full_pool_bytes':648,'context_bytes':32,'rng_bytes':4,'routing_bytes':160,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware timing','limitations':['Harness explicitly schedules events and one selected voice render per record. Not the original allocator, cadence, note/pitch preparation, envelope routing, mixing or decimation.','Only known normalization counts 1,4,5,6,7 selected; counts 2,3,8 unresolved. Known tables supplied explicitly.','Only valid original voice indices and sound slots tested. C++ invalid-input guards are adapter behavior, not original assert emulation.','No plugin integration, presets or installer changes.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: Cluster Core callbacks, six-slot pools and 131072 interleaved sample outputs reproduced')
if __name__=='__main__':main()
