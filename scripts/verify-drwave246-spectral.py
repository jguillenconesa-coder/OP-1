#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Run six original Dr. Wave spectral callbacks against generated base banks.
Arguments, gains, replace/add mode and callback order are explicit test inputs.
This does not reconstruct the complete original control/update dispatcher.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES=768
RECORD=1072
RESULT=6196
BANK=163840
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    seed=0x53504543
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    data=bytearray(struct.pack('<1536I',*[rnd() for _ in range(1536)]))
    for case in range(CASES):
        independent=case<384;block=(case-384)%64
        action=int(independent or block==0)|(2 if independent or block%11==0 else 0)
        family=case%6;slot=case//6%6;step=case//6%64
        argument=step*65536+(0,1,32767,32768,65535)[step%5] if independent else rnd()%(64*65536)
        if not independent and block%8==0:argument=(0,1,65535,65536,65537,64*65536-1)[block//8%6]
        gain=(0,1,8192,16384,32768,65535,65536)[case//6%7]
        boost=(0,65536,262144,1,65535)[case//42%5]
        flags=(rnd()&0xffffff00)|(0,1,128,255)[case//6%4]
        wave=[rnd() for _ in range(256)]
        data+=struct.pack('<4I4I256I4I',argument,family,slot,action,flags,gain,32768,boost,*wave,*[rnd() for _ in range(4)])
    if len(data)!=6144+CASES*RECORD:raise RuntimeError('Invalid spectral input size')
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
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-spectral.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=2*BANK+CASES*RESULT:raise RuntimeError('Incomplete original spectral output')
    if output[:BANK]!=output[-BANK:]:raise RuntimeError('Spectral callbacks mutated the generated banks')
    artifacts={}
    for name,path in [('drwave246-spectral-input.bin',out/'inputs.bin'),('drwave246-spectral-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Spectral oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'callbacks':dict(zip(['all_up','all_down','odd_up','odd_down','triangle_up','triangle_down'],['0x0197ae28','0x0197b1bc','0x0197b55c','0x0197b944','0x0197bd20','0x0197c1b0'])),
            'argument_domain':[0,64*65536-1],'working_gain_domain':[0,65536],'fixed_reference_gain':32768,'output_gain_domain':[0,262144],
            'settings_address':'0x01cc1864','wave_pointer':'0xff906a94','banks':'0x01cc208c','banks_bytes':BANK,
            'sine_source':'0x018b6688','sine_runtime':'0x0101e5e8','sine_contract':'Explicit low-word copy of previously verified source; not execution of the full sine initializer.',
            'bank_initialization':'Original shared initializer then complete base-bank builder, once before the callbacks; verified unchanged afterward.',
            'independent_cases':384,'continuous_streams':6,'blocks_per_stream':64,'cases':CASES,'calls_per_callback':128,'spectral_wave_words':CASES*256,
            'artifacts':artifacts,'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
            'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'limitations':['Callback argument, gains, replace/add flag, slot and schedule are explicit, bounded test inputs. They do not implement the original knob preparation or complete update dispatcher.',
                           'No full runtime voice, pitch provider, ADSR, output or plugin integration is claimed. The transforms and five-oscillator reader remain separately verified blocks.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: six original spectral callbacks, 768 calls / 196608 wave words; generated banks preserved')
if __name__=='__main__':main()
