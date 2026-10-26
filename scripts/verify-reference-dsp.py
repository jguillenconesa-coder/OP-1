#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated Blackfin oracles with operator-supplied firmware/tools.
Writes original instruction snippets ONLY into a new directory outside this repo.
Runs generated simulator executables, not the OP-1 boot/system firmware.
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

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(args,**kwargs):return subprocess.run([str(x) for x in args],check=True,timeout=60,**kwargs)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ldr',required=True,type=Path);p.add_argument('--assembler',required=True,type=Path)
    p.add_argument('--linker',required=True,type=Path);p.add_argument('--simulator',required=True,type=Path)
    p.add_argument('--testutils',required=True,type=Path,help='GDB simulator testsuite/bfin directory')
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();out=a.output.resolve()
    if out.is_relative_to(ROOT) or out.exists():raise ValueError('Choose a new output directory outside the repository')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if digest(a.ldr)!=expected:raise ValueError('This address profile requires the inventoried revision 246 LDR')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    metadata,image=mapper.inspect(a.ldr.read_bytes());base=int(metadata['sdram_start'],16)
    out.mkdir(parents=True)
    for folder in ['fm-probe','q15-probe','pd-probe','pd-full-probe','lut-probe']:(out/folder).mkdir()
    for folder,name,start,end in [('fm-probe','prefix.bin',0x019a23d4,0x019a2452),('q15-probe','multiply.bin',0x01a0bd1a,0x01a0bd1e),('pd-probe','kernel.bin',0x01a0bc5c,0x01a0be82),('pd-full-probe','kernel.bin',0x01a0bc5c,0x01a0beaa),('lut-probe','init.bin',0x01a49b1c,0x01a49b42)]:
        (out/folder/name).write_bytes(image[start-base:end-base])
    values=[-32768,-32767,-16384,-1,0,1,16384,32766,32767];pairs=[(x,y) for x in values for y in values];state=0x504f434b
    while len(pairs)<65536:
        state=(1664525*state+1013904223)&0xffffffff;x=(state&65535)-32768
        state=(1664525*state+1013904223)&0xffffffff;y=(state&65535)-32768;pairs.append((x,y))
    (out/'q15-probe/inputs.bin').write_bytes(b''.join(struct.pack('<hh',*pair) for pair in pairs))
    table=[round(math.sin((i-4096)*math.pi/4096)*32767) for i in range(8192)]
    (out/'pd-probe/table.bin').write_bytes(struct.pack('<8192h',*table));state=0x50484153;cases=[]
    for i in range(1024):
        state=(1664525*state+1013904223)&0xffffffff;phase=int(state%3000000000)-1500000000
        state=(1664525*state+1013904223)&0xffffffff;increment=state%1000000+1;gain=(i*719)%65536-32768
        cases.append(struct.pack('<4h',0,gain,0,0)+bytes(124)+struct.pack('<iI',phase,increment))
    (out/'pd-probe/inputs.bin').write_bytes(b''.join(cases))
    # Recover this specific read-only table, interpreting intersecting fill
    # arguments as repeated little-endian 32-bit words. The legacy generic map
    # intentionally still omits fills; do not silently alter its address profile.
    ldr=a.ldr.read_bytes();table_source=bytearray(8193*4);covered=bytearray(len(table_source));offset=0;fill_count=0
    table_base=0x018b6688
    while offset<len(ldr):
        flags,address,count,argument=struct.unpack_from('<4I',ldr,offset)
        low=max(address,table_base);high=min(address+count,table_base+len(table_source))
        if high>low:
            if flags&0x100:
                shift=(low-address)%4;length=high-low
                part=(struct.pack('<I',argument)*((length+shift+3)//4))[shift:shift+length];fill_count+=1
            else:part=ldr[offset+16+low-address:offset+16+high-address]
            table_source[low-table_base:high-table_base]=part;covered[low-table_base:high-table_base]=bytes([1])*(high-low)
        offset+=16+(0 if flags&0x100 else count)
    verified=[round(math.sin(i*math.pi/4096)*32767) for i in range(8193)]
    observed=[((word&65535)^32768)-32768 for word in struct.unpack('<8193I',table_source)]
    if not all(covered) or observed!=verified:raise RuntimeError('Original LUT source differs from the independently calculated sine')
    (out/'lut-probe/source.bin').write_bytes(table_source)
    (out/'pd-full-probe/table.bin').write_bytes(struct.pack('<8193h',*verified))
    state=0x50444655;cases=[];bounds=[0,1,16384,32767,-1,-32768,-32767,8192]
    def random_word():
        nonlocal state
        state=(1664525*state+1013904223)&0xffffffff
        return state
    for i in range(8192):
        controls=[bounds[(i//(8**j))%8] if i<4096 else (random_word()&65535)-32768 for j in range(4)]
        phase=(random_word()^0x80000000)-0x80000000
        increment=[0,1,100000,10000000,0x1fffffff,0x40000000,0x7fffffff][i%7]
        secondary=(random_word()^0x80000000)-0x80000000
        cases.append(struct.pack('<4h',*controls)+bytes(124)+struct.pack('<iIiII',phase,increment,secondary,i%9,i%3))
    (out/'pd-full-probe/inputs.bin').write_bytes(b''.join(cases))
    templates=ROOT/'research/blackfin'
    for folder,template,script,regions in [('fm-probe','fm-fields.s.in','flat.ld',['0xff904000,0x4000']),('q15-probe','multiply.s.in','flat.ld',[]),('pd-probe','pd-steady.s.in','pd.ld',['0xff800000,0x8000']),('pd-full-probe','pd-transient.s.in','pd.ld',['0xff800000,0x8000']),('lut-probe','lut-copy.s.in','lut.ld',[])]:
        path=out/folder;source=path/'probe.s';source.write_text((templates/template).read_text().replace('@WORK@',out.as_posix()))
        run([a.assembler,'-I',a.testutils,'-o',path/'probe.o',source]);run([a.linker,'-T',templates/script,'-o',path/'probe.elf',path/'probe.o'])
        command=[a.simulator,'--model','bf524','--memory-size','64M','--watch-cycles-int','300000000']
        for region in regions:command+=['--memory-region',region]
        with (path/'results.bin').open('wb') as stdout,(path/'sim.log').open('wb') as stderr:run(command+[path/'probe.elf'],stdout=stdout,stderr=stderr)
    comparisons={'fm-fields.bin':'fm-probe/results.bin','q15-input.bin':'q15-probe/inputs.bin','q15-output.bin':'q15-probe/results.bin','pd-input.bin':'pd-probe/inputs.bin','pd-output.bin':'pd-probe/results.bin','synthetic-sine.bin':'pd-probe/table.bin','pd-transient-input.bin':'pd-full-probe/inputs.bin','pd-transient-output.bin':'pd-full-probe/results.bin','verified-sine.bin':'pd-full-probe/table.bin','lut-copy-output.bin':'lut-probe/results.bin'}
    result={'ldr_sha256':expected,'model':'GNU simulator BF524; user-mode explicit RAM; not hardware validation','artifacts':{},'tools':{str(x):digest(x) for x in [a.assembler,a.linker,a.simulator]}}
    for fixture,generated in comparisons.items():
        actual=out/generated;reference=ROOT/'Tests/ReferenceVectors'/fixture
        if actual.read_bytes()!=reference.read_bytes():raise RuntimeError('Oracle differs: '+fixture)
        result['artifacts'][fixture]={'bytes':actual.stat().st_size,'sha256':digest(actual),'matches_committed_fixture':True}
    copy_result=(out/'lut-probe/results.bin').read_bytes()
    expected_shifted=struct.pack('<8193h',*(verified[1:]+[verified[-1]]))
    if copy_result!=expected_shifted:raise RuntimeError('LUT-copy timing differs from the recorded GNU simulator result; investigate before accepting new vectors')
    result['lut_source']={'entries':8193,'intersecting_fill_blocks':fill_count,'source_sha256':hashlib.sha256(table_source).hexdigest(),'matches_mathematical_sine':True,'simulator_initializer_shift_entries':1,'hardware_timing_verified':False}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: reproduced all ten oracle files byte-for-byte; original instructions retained only in '+str(out))
if __name__=='__main__':main()
