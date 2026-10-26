#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated Digital rev246 outputs using operator-supplied firmware.
Original instructions/ELF remain in a NEW private directory outside the repo.
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
    data=bytearray();seed=0x44494749
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    for i in range(1024):
        state=struct.pack('<8I',*[rnd() for _ in range(8)])
        c=[rnd()%32768,2048+rnd()%24577,int(rnd()%65536)-32768,rnd()%32768]
        if i<256:c=[(0,1,16384,32767)[i%4],2048+(i//4%7)*4096,(-32768,-1,0,1,32767)[i//28%5],(0,32767)[i//140%2]]
        pitch=[rnd() if i%8==0 else rnd()%0x10000000 for _ in range(128)]
        data+=state+struct.pack('<4h128II',*c,*pitch,1)
    patches=json.loads((ROOT/'research/blackfin/digital-controls.json').read_text())['presets']
    for patch in patches:
        for block in range(64):
            pitch=[round(440*2**((48-69+12*math.sin(2*math.pi*3*(block*128+j)/88200))/12)/88200*2**32) for j in range(128)]
            controls=patch['controls'].copy()
            if block>=32:controls[0]=(controls[0]+block*512)%32768;controls[2]=max(-32768,min(32767,-controls[2]))
            data+=bytes(32)+struct.pack('<4h128II',*controls,*pitch,3 if block==0 else 2 if block==32 else 0)
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--record',action='store_true',help='Record numeric fixtures after verifying the input firmware hash')
    a=p.parse_args();out=a.output.resolve();templates=ROOT/'research/blackfin'
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW private directory outside the repository')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only the inventoried rev246 LDR is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);offset=0
    while offset<len(raw):
        flags,address,count,argument=struct.unpack_from('<4I',raw,offset)
        if base<=address and address+count<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+count]=(struct.pack('<I',argument)*((count+3)//4))[:count] if flags&0x100 else raw[offset+16:offset+16+count]
        offset+=16+(0 if flags&0x100 else count)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);data=inputs();blocks=len(data)//556
    (out/'inputs.bin').write_bytes(data)
    source=(templates/'digital-stream.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(blocks*288))
    (out/'probe.s').write_text(source)
    def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=60,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','300000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=blocks*288:raise RuntimeError('Incomplete Digital output')
    artifacts={}
    for name,path in [('digital246-input.bin',out/'inputs.bin'),('digital246-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path),'matches_committed_fixture':True}
    evidence={'ldr_sha256':expected,'model':'GNU 13.1 BF524 with documented local D-register writeback patch; not hardware validation','render':'0x01a51f7a','reset':'0x01a521e2','constructor':'0x01a51f28','sine_source':'0x018b6688','sine_runtime':'0x0101e5e8','input_pitch':'0xff801950','internal_rate_hz':88200,'random_blocks':1024,'stored_preset_streams':len(json.loads((templates/'digital-controls.json').read_text())['presets']),'blocks':blocks,'samples':blocks*128,'complete_states':blocks,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patch_sha256':sha(templates/'gnu-13.1-dreg-writeback.patch'),'limitations':['Isolated oscillator only; original full note preparation, FX, LFO, velocity, mixing and hardware timing are not validated.','Production rate conversion, filtering, gain and per-voice envelope integration are independently designed.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'PASS: {blocks*128} Digital sample outputs and {blocks} complete states reproduced')
if __name__=='__main__':main()
