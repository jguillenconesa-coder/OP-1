#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast Cluster's shared initializer and its original Q15/Q31 conversions.
All eight initializer source words are explicit test inputs. Three ORIGINAL
normalization words remain unknown; this probe does not recover or infer them.
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
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true')
    a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a new private output directory outside the repository')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0;covering=[]
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if address<=0xff900524<address+n:covering.append({'header_offset':hex(cursor),'flags':hex(flags),'address':hex(address),'bytes':n})
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    seed=0x53485244
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    observed=json.loads((ROOT/'research/blackfin/cluster246-contract.json').read_text())['normalization']['observed_entries']
    scalars=[sign<<31|exponent<<23|fraction for sign in [0,1] for exponent in range(256) for fraction in [0,1,0x3fffff,0x400000,0x7ffffe,0x7fffff]]
    scalars.extend(int(row['float32_bits'],16) for row in observed)
    scalars.extend(struct.unpack_from('<24I',image,0x018befb0-base))
    while len(scalars)<65536:scalars.append(rnd())
    scalar_data=struct.pack('<65536I',*scalars)
    shared_data=bytearray()
    for case in range(256):
        source=[rnd() for _ in range(8)]
        if case<64:
            for row in observed:source[row['count']-1]=int(row['float32_bits'],16)
        memory=bytes([[0,255,85,170][case%4]])*64 if case<32 else struct.pack('<16I',*[rnd() for _ in range(16)])
        shared_data+=struct.pack('<8I',*source)+memory+struct.pack('<I32I',rnd(),*[rnd() for _ in range(32)])
    if len(shared_data)!=256*228:raise RuntimeError('Unexpected shared input dimensions')
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'scalar-inputs.bin').write_bytes(scalar_data);(out/'shared-inputs.bin').write_bytes(shared_data)
    templates=ROOT/'research/blackfin'
    source=(templates/'cluster-shared.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@SCALAR_BYTES@',str(65536*8)).replace('@SHARED_BYTES@',str(256*196))
    (out/'probe.s').write_text(source)
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=120,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    result=(out/'output.bin').read_bytes()
    if len(result)!=65536*8+256*196:raise RuntimeError('Incomplete scalar/shared output')
    (out/'scalar-output.bin').write_bytes(result[:65536*8]);(out/'shared-output.bin').write_bytes(result[65536*8:])
    artifacts={}
    for name,path in [('fractional246-input.bin',out/'scalar-inputs.bin'),('fractional246-output.bin',out/'scalar-output.bin'),('cluster246-shared-input.bin',out/'shared-inputs.bin'),('cluster246-shared-output.bin',out/'shared-output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    evidence={'ldr_sha256':expected,'shared_initializer':'0x019608e0','q15_helper':'0x01aa7014','q31_helper':'0x01aa6fd0','scalar_cases':65536,'scalar_conversions':131072,'directed_exponent_sign_fraction_cases':3072,'shared_cases':256,'shared_image_base':'0x01baa910','shared_image_bytes':64,'rng_bytes':4,'damping_bytes':128,'initializer_source_contract':'All eight IEEE binary32 words supplied explicitly. Known five source words retained in first 64 test records; other entries are synthetic test values, never recovered originals.','unresolved_original_counts':[2,3,8],'covering_normalization_source_ldr_blocks':covering,'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware/boot validation','artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Original table is still partial; complete module initialization under test inputs does not supply three missing L1 values.','No allocator, cadence or full plugin integration claimed. No presets or video used.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: 131072 original fractional conversions and 256 complete shared initialization cases reproduced')
if __name__=='__main__':main()
