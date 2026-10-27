#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated Cluster rev246 outputs using operator-supplied firmware.
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
    damping_q31=json.loads((ROOT/'research/blackfin/cluster246-contract.json').read_text())['damping']['verified_q31']
    data=bytearray();seed=0x434c5354
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    for i in range(2048):
        state=bytearray(struct.pack('<27I',*[rnd() for _ in range(27)]))
        state[106]=i%2
        controls=[2048+rnd()%16384,rnd()%32768,rnd()%32768,rnd()%32768]
        if i<256:
            controls=[2048+(i%8)*2048+(0,2047)[i//8%2],
                      (0,1,6553,6554,13107,19660,26214,32767)[i//16%8],
                      i%32*1024,(0,1,16384,32767)[i//64%4]]
        if i<512:
            action=1|(2 if i%4==0 else 0)|(4 if i%7==0 else 0)|(8 if i%3==0 else 0)
            pitch=[rnd() if i%8==0 else rnd()%0x10000000 for _ in range(128)]
        else:
            block=(i-512)%64;stream=(i-512)//64
            action=(3 if block==0 else 4 if block==40 else 0)|(8 if block%5==0 else 0)
            controls=[3072+(stream%8)*2048, int(block*32767/63), 512+(stream%24)*1024, 3+(stream%4)*545]
            pitch=[round((55*2**(stream%6))*2**(0.01*math.sin((block*128+j)/1000))/88200*2**32) for j in range(128)]
        norms=[rnd()%32768 for _ in range(8)]
        damping=[rnd() for _ in range(32)]
        if i>=512:
            norms=[8192+(stream*977+j*1297)%24576 for j in range(8)]
            damping=damping_q31+[0]*8
        context=struct.pack('<8I',*[rnd() for _ in range(8)])
        data+=state+struct.pack('<4h128I8h32II',*controls,*pitch,*norms,*damping,rnd())+context+struct.pack('<I',action)
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
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);data=inputs();blocks=len(data)//812
    (out/'inputs.bin').write_bytes(data)
    conversion=''.join(f'imm32 P0, {0x18befb0+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0x106a954+i*4}\n[P0] = R0;\n' for i in range(24))
    source=(templates/'cluster-stream.s.in').read_text().replace('@COEFFICIENTS@',conversion).replace('@WORK@',out.as_posix()).replace('@BYTES@',str(blocks*400))
    (out/'probe.s').write_text(source)
    def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=120,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=160+blocks*400:raise RuntimeError('Incomplete Cluster output')
    observed=(out/'output.bin').read_bytes()
    contract=json.loads((templates/'cluster246-contract.json').read_text())
    for i,c in enumerate(contract['control_ranges']):
        if struct.unpack_from('<4h',observed,i*8)!=(c['min'],c['max'],c['scale'],c['kind']):raise RuntimeError('Original control descriptor differs')
    if list(struct.unpack_from('<24i',observed,64))!=contract['damping']['verified_q31']:raise RuntimeError('Original damping conversion differs')
    artifacts={}
    for name,path in [('cluster246-input.bin',out/'inputs.bin'),('cluster246-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if a.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path),'matches_committed_fixture':True}
    evidence={'ldr_sha256':expected,'model':'GNU BF524 with documented D-register deferred writeback patch; not hardware validation',
        'damping_source':'0x018befb0','damping_conversion':'0x01aa6fd0','verified_damping_words':24,
        'render':'0x01960288','reset':'0x01960664','release':'0x01960248','display_decay':'0x0196089c',
        'self_bytes':108,'context_bytes':32,'random_seed_bytes':4,'independent_blocks':512,'stream_blocks':1536,'blocks':blocks,'samples':blocks*128,
        'coefficient_contract':'512 raw-kernel stress blocks with injected normalization/damping; 1536 continuous blocks with verified original damping and fixed synthetic count coefficients per stream. No original/default L1 normalization values claimed.',
        'l1_dependency':{'address':'0xff900524','ldr_block_offset':'0x1fe870','flags':'0xad812406','status':'CALLBACK|INDIRECT payload has no validated decrypted image; original initialization coefficients pending'},
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patch_sha256':sha(templates/'gnu-13.1-dreg-writeback.patch'),
        'limitations':['Isolated coefficient-parameterized kernel, reset/release/display callbacks only; no plugin integration or original default coefficient table claimed.',
                       'Original note preparation, allocation, cadence, envelope/routing/output and hardware timing remain pending. No preset or video data used.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'PASS: {blocks*128} Cluster sample outputs and {blocks} full states/context/seed reproduced with caller coefficients')
if __name__=='__main__':main()
