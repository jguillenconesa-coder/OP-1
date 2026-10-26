#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated Punch rev246 outputs using operator-supplied firmware.
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
    data=bytearray();seed=0x50554e43
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    for i in range(512):
        state=struct.pack('<50IhhI',*[rnd() for _ in range(50)],int(rnd()%65536)-32768,int(rnd()%65536)-32768,i%4)
        controls=[1344+rnd()%11137,rnd()%32768,1536+rnd()%23553,rnd()%32768]
        if i<128:controls=[(1344,6000,12480)[i%3],(0,16384,32767)[i//3%3],1536+(i//9%24)*1024,(0,1,16384,32767)[i//216%4]]
        audio=[(0,32767,-32768,int(rnd()%65536)-32768)[j%4] for j in range(64)]
        data+=state+struct.pack('<4h64hI',*controls,*audio,1)
    patches=json.loads((ROOT/'research/blackfin/punch-controls.json').read_text())['presets']
    for patch in patches:
        for block in range(512):
            audio=[round(20000*math.sin(2*math.pi*220*(block*64+j)/44100)) if block<256 else 0 for j in range(64)]
            controls=patch['controls'].copy()
            if 128<=block<256:controls[2]=1536+(block%24)*1024
            data+=bytes(208)+struct.pack('<4h64hI',*controls,*audio,2 if block in (0,448) else 0)
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
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);data=inputs();blocks=len(data)//348
    (out/'inputs.bin').write_bytes(data)
    setup=''
    for i in range(129):setup+=f'imm32 P0, {0x18be744+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0xff801f50+i*4}\n[P0] = R0;\n'
    source=(templates/'punch-stream.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(blocks*340)).replace('@SETUP@',setup)
    (out/'probe.s').write_text(source)
    def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=60,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','300000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=540+blocks*340:raise RuntimeError('Incomplete Punch output')
    artifacts={}
    for name,path in [('punch246-input.bin',out/'inputs.bin'),('punch246-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path),'matches_committed_fixture':True}
    evidence={'ldr_sha256':expected,'model':'GNU 13.1 BF524 with documented local D-register writeback patch; not hardware validation','render':'0x019e6260','reset':'0x019e6160','coefficient_float_source':'0x018be744','coefficient_runtime':'0xff801f50','conversion':'0x01aa6fd0','range_initializer':'0x01a1e914','range_output':'0x01de5cb0','sine_source':'0x018b6688','sine_runtime':'0x0101e5e8','assumed_effect_rate_hz':44100,'random_blocks':512,'stored_preset_streams':3,'blocks':blocks,'samples':blocks*64,'complete_states':blocks,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patch_sha256':sha(templates/'gnu-13.1-dreg-writeback.patch'),'limitations':['Isolated mono effect with original converted coefficients; original complete preset routing and hardware timing are not validated.','44.1 kHz effect clock is an integration assumption supported by the 64-frame effect/128-frame voice contract, not an independent codec clock measurement.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'PASS: {blocks*64} Punch sample outputs and {blocks} complete states reproduced')
if __name__=='__main__':main()
