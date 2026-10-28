#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Compare the three original Dr. Wave post-transform routines in isolation.
Internal Q16 words, buffers and scheduling are explicit; presets/knob routing
are not recovered by this probe. Original code and ELF remain private.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES=512
RECORD=3188
RESULT=8300
SEEDS=(32768,29127,26214,23831,21845,20164,18724,17476)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    seed=0x5452414e
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    prefix=bytearray()
    for slot in range(6):
        prefix+=struct.pack('<256I',*[(rnd() if slot%2 else (i*7919+slot*0x12345678)&0xffffffff) for i in range(256)])
    data=bytearray(prefix)
    edges=sorted({max(0,min(65536,n*65536//7+d)) for n in range(8) for d in [-2,-1,0,1,2]}|{0,1,65535,65536,32767,32768})
    for case in range(CASES):
        independent=case<256;block=(case-256)%64
        action=int(independent or block==0)|(2 if independent or block%7==0 else 0)|(4 if case%11==0 else 0)
        mode=case%3;slot=(case//3)%6
        word=edges[case//3%len(edges)] if case<256 else rnd()%65537
        if case>=256 and block%16==0:word=(0,65536)[block//16%2]
        wave=[(rnd() if case%3 else ((i+case)<<16)|(255-i)) for i in range(256)]
        cache=bytearray(struct.pack('<520I',*[rnd() for _ in range(520)]))
        flag=(0,1,7,0xffffffff)[case//3%4]
        struct.pack_into('<2I',cache,4,(0,1,7,0xffffffff)[case//9%4],(0,1,7,0xffffffff)[case//27%4])
        if flag:struct.pack_into('<8H',cache,12,*SEEDS)
        config=[rnd() for _ in range(8)];config[3 if mode==0 else 5]=word
        data+=struct.pack('<4I256I',action,mode,slot,word,*wave)+cache+struct.pack('<I8I8I',flag,*config,*[rnd() for _ in range(8)])
    if len(data)!=6144+CASES*RECORD:raise RuntimeError('Invalid transform input size')
    return data
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--record',action='store_true');args=parser.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py')
    mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16)
    image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,argument=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',argument)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    constants=[struct.unpack_from('<q',image,address-base)[0] for address in [0x018d50b0,0x018d50b8,0x018d50c0]]
    if constants!=[7*65536]*3:raise RuntimeError('Unexpected original rate constants')
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-transform.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=CASES*RESULT:raise RuntimeError('Incomplete original transform output')
    artifacts={}
    for name,path in [('drwave246-transform-input.bin',out/'inputs.bin'),('drwave246-transform-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original transform oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'functions':{'scatter_hold':'0x0197a8a0','clamp':'0x0197ab74','repeat':'0x0197ac88'},
            'explicit_word_domain':[0,65536],'configuration_pointer':'0xff906a90','wave_pointer':'0xff906a94','private_configuration_object':'0x02f00000',
            'scatter_config_offset':12,'clamp_repeat_config_offset':20,'rate_constants':constants,
            'cache_region':{'start':'0x01cc1870','bytes':2080,'includes_guards':True},'reciprocal_init_flag':'0x01b60eec',
            'independent_cases':256,'continuous_streams':4,'blocks_per_stream':64,'cases':CASES,'transformed_wave_words':CASES*256,
            'artifacts':artifacts,'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
            'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'limitations':['The internal word and choice/order of three transform callbacks are explicit inputs; the original knob conversion and complete wave-update dispatcher remain pending.',
                           'No spectral-family mixing, original voice scheduler, pitch provider, ADSR, output or plugin integration is completed by these transforms.',
                           'The supported nonnegative Q16 domain avoids out-of-range addressing branches. No arbitrary-word emulation or hardware timing claim.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: 512 original transform calls, 131072 wave words and full six-wave/cache/guard/config state recorded')
if __name__=='__main__':main()
