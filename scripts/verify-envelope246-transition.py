#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce ADSR initialization/restart callbacks and their following renders.
Original firmware/ELF stay in a new private directory outside checkout.
This does not establish Cluster's full envelope/pitch/mixer scheduling.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    data=bytearray();seed=0x5452414e
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    for case in range(2560):
        independent=case<512;block=(case-512)%128
        if independent:action=case%6
        else:action={0:2,16:5,17:5,48:3,64:5,80:2,100:4,110:5}.get(block,0)
        if independent or block==0:action|=256
        levelIndex=(case//54)%9 if independent else case%9
        level=(0,1,16,127,128,129,1073741823,1073741824,rnd()%1073741825)[levelIndex]
        gain=(rnd()&0xffff0000)|(rnd()%32768)
        stage=(case//6)%9 if independent else case%9
        state=[stage,rnd(),level,rnd(),rnd(),gain]
        controls=[rnd()%16384,rnd()%16384,rnd()%32768,rnd()%16384]
        if not independent:
            stream=(case-512)//128
            controls=[(0,1024,4096,8192)[stream%4],(0,4096,8192,16383)[stream//4],
                      (0,1,8192,32767)[stream%4],(0,4096,8192,16383)[stream%4]]
        volume=(0,1,16384,32767)[case%4] if case<256 else rnd()%32768
        pending16=(0,1,32767,32768,65535)[case%5] if case<256 else rnd()%65536
        data+=struct.pack('<6I4h2HI2H3I',*state,*controls,volume,controls[2],rnd(),pending16,0,rnd(),rnd()%65536-32768&0xffffffff,action)
    if len(data)!=2560*56:raise RuntimeError('Invalid transition record size')
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW private directory outside checkout')
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
    conversion=''.join(f'imm32 P0, {0x018c13c4+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0x0106b3cc+i*4}\n[P0] = R0;\n' for i in range(256))
    source=(templates/'adsr-transition.s.in').read_text().replace('@COEFFICIENTS@',conversion).replace('@WORK@',out.as_posix()).replace('@BYTES@',str(2560*316))
    (out/'probe.s').write_text(source)
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=120,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=2560*316:raise RuntimeError('Incomplete transition output')
    artifacts={}
    for name,path in [('adsr246-transition-input.bin',out/'inputs.bin'),('adsr246-transition-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    evidence={'ldr_sha256':expected,'initialize':'0x019120ba','request_restart':'0x019120fc','note_on':'0x0191211a','release':'0x019120d0','fast_release':'0x0191209c','render':'0x01912128','coefficient_conversion':'0x01aa6fd0','source_coefficients':['0x018c13c4','0x018c15c4'],'independent_cases':512,'continuous_streams':16,'blocks_per_stream':128,'blocks':2560,'samples':327680,'state_before_after_bytes':24,'input_record_bytes':56,'output_record_bytes':316,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with documented D-register deferred writeback patch; no hardware timing validation','limitations':['Events, pending words, gain, volume and controls are explicit test inputs; not the complete VoiceCtl/Cluster/pitch/mixer schedule.','Source addresses of restart inputs are distinct and outside the envelope state; pointer aliasing not covered.','Valid first-four ADSR ranges and nonnegative level/gain/volume tested. No semantics inferred for the four auxiliary preset controls or pending-field runtime consumers.','No presets, audio from videos or plugin integration added.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: 2560 ADSR callback/render cases, 327680 samples and full state/dither records reproduced')
if __name__=='__main__':main()
