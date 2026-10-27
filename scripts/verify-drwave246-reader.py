#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Run the unmodified rev246 Dr. Wave reader with explicit synthetic waves.

Only non-update slots are rendered: the scheduled wave construction is a
separate, unported route. Original instructions and ELF remain private.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES=2048
RECORD=1072
RESULT=536
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    seed=0x44525756
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    waves=[]
    for slot in range(6):
        for index in range(256):
            value=((index-128)*4096,(32767 if index%2 else -32768)*16,
                   0x7fffffff if index==0 else 0,0xffffffff if index<128 else 0,
                   ((index*7919)^0x98765432),rnd())[slot]
            waves.append(value&0xffffffff)
    data=bytearray(struct.pack('<1536I',*waves))
    for case in range(CASES):
        independent=case<1024
        block=(case-1024)%64
        action=int(independent or block==0)|(2 if case%7==0 else 0)|(4 if case%17==0 else 0)|(8 if case%11==0 else 0)
        voice=case%6
        update_slot=(voice+1+(case//6)%5)%6
        update_counter=update_slot+6*(rnd()%5461)
        if case%31==0 and voice!=1:update_counter=32767
        enabled=int(case%13!=0)
        orange=(0,1,127,128,255,256,8192,16384,32766,32767)[case%10] if case<256 else rnd()%32768
        controls=[rnd()%65536-32768 for _ in range(3)]+[orange]
        pitch=[rnd() for _ in range(128)]
        if case<256:pitch[0]=(0,1,255,256,0x7fffffff,0x80000000,0xffffff00,0xffffffff)[case//10%8]
        pool=[]
        for slot in range(6):
            phase=[rnd()&0xffffff for _ in range(5)]
            if case<256:phase=[(0,1,65535,65536,0xffffff)[(case+i)%5] for i in range(5)]
            pool.extend([rnd(),rnd(),*phase,slot,rnd()])
        output=[rnd() for _ in range(68)]
        context=[rnd() for _ in range(5)]
        cache=[0,*[rnd() for _ in range(4)]]
        data+=struct.pack('<5I4h128I54I68I5I5II',action,voice,update_counter,enabled,0,*controls,*pitch,*pool,*output,*context,*cache,rnd())
    if len(data)!=6144+CASES*RECORD:raise RuntimeError('Wrong input dimensions')
    return data

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--record',action='store_true')
    args=parser.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside the checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py')
    mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw)
    base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,argument=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',argument)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-reader.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes();source=(out/'inputs.bin').read_bytes()
    if len(output)!=CASES*RESULT+6144:raise RuntimeError('Incomplete original output')
    if output[-6144:]!=source[:6144]:raise RuntimeError('Original reader mutated synthetic waves')
    artifacts={}
    for name,path in [('drwave246-reader-input.bin',out/'inputs.bin'),('drwave246-reader-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    rendered=sum(struct.unpack_from('<I',source,6144+i*RECORD+12)[0]!=0 for i in range(CASES))
    evidence={'ldr_sha256':expected,'render':'0x0197c870','reset':'0x0197a826','release':'0x0197a838','factory':'0x0197ada8',
              'vtable':'0x0190fb2c','state_bytes':36,'slots':6,'oscillators':5,'phase_mask':'0x00ffffff',
              'synthetic_waves_address':'0x01cea08c','shared_increments_address':'0x01ceb88c',
              'reciprocal_seed_source':'0x018d5100','reciprocal_seed_u16':list(struct.unpack_from('<8H',image,0x018d5100-base)),
              'independent_cases':1024,'continuous_streams':16,'blocks_per_stream':64,'cases':CASES,
              'rendered_blocks':rendered,'inactive_blocks':CASES-rendered,'rendered_samples':rendered*128,
              'factory_calls':sum(i%7==0 for i in range(CASES)),'reset_calls':sum(i%17==0 for i in range(CASES)),
              'release_calls':sum(i%11==0 for i in range(CASES)),
              'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
              'artifacts':artifacts,'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
              'limitations':['All six wave buffers are synthetic caller inputs. No original waveform tables or presets are recovered by this test.',
                             'The explicit nonnegative signed16 update counter designates a wave-update slot modulo six. Only other slots or the inactive early return are executed; wave generation remains pending.',
                             'Only pitch[0] is consumed by this route. The external scheduler, pitch provider, ADSR, mixer and plugin integration remain pending.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'PASS: {CASES} original Dr. Wave reader records, {rendered*128} samples; synthetic waves unchanged')
if __name__=='__main__':main()
