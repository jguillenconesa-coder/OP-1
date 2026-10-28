#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast tuning-index arithmetic and Dr. Wave activation with explicit indexed
coefficients. Neither the coefficient table nor the render-buffer writer is
recovered. Original instructions and ELF stay in a fresh private directory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
INDEX_CASES=65536+23*199+256
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
def index_inputs():
    data=bytearray();seed=0x494e4458
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    bounds=(0,1,0x7fffffff,0x80000000,0xffffffff,0x7ffffff4,0xfffffff4,0x40000000)
    for case in range(INDEX_CASES):
        if case<65536:
            a,b=(bounds[case//8],bounds[case%8]) if case<64 else (rnd(),rnd())
        elif case<65536+23*199:
            i=case-65536;a=i//199-11;b=i%199-99
        else:a,b=bounds[case%8],bounds[(case//8)%8]
        data+=struct.pack('<8I',int(case<=65536+23*199),a&0xffffffff,b&0xffffffff,*[rnd() for _ in range(5)])
    return data
def activation_inputs(base):
    activation=module('drwave_activation',ROOT/'scripts/verify-drwave246-activation.py');events,expanded=activation.inputs(base)
    changes=bytearray();records=bytearray();seed=0x544e494e
    directed=((-11,-99),(11,99),(0,0),(-11,99),(11,-99),(-1,99),(1,-99))
    for case in range(512):
        seed=(seed*1664525+1013904223)&0xffffffff;coarse=int(seed%23)-11
        seed=(seed*1664525+1013904223)&0xffffffff;fine=int(seed%199)-99
        if case<len(directed):coarse,fine=directed[case]
        tail=struct.pack('<2i',coarse,fine)
        changes+=events[case*232:(case+1)*232]+tail
        records+=expanded[case*9536:(case+1)*9536]+tail
    return changes,records
def activation_template():
    text=(ROOT/'research/blackfin/drwave-activation.s.in').read_text()
    init='''// Explicit zero-filled test coefficient table, not original calibration.
imm32 P1, 0x027fc720
P3 = 2400;
R0 = 0;
LSETUP(tuning_zero,tuning_zero_end) LC0 = P3;
tuning_zero: [P1++] = R0;
tuning_zero_end: NOP;
imm32 P1, 0x018c217c
imm32 R0, 0x11112222
[P1++] = R0;
imm32 R0, 0x33334444
[P1++] = R0;
imm32 R0, 0x55556666
[P1++] = R0;
imm32 R0, 0x77778888
[P1] = R0;
'''
    text=text.replace('CALL original_note_copy;',init+'CALL original_note_copy;',1)
    call='''no_reload:
P0 = 9536;
P1 = P4 + P0;
R0 = [P1++];
R1 = [P1];
imm32 P0, 0x01a842f0
CALL (P0);
'''
    text=text.replace('no_reload:\n',call,1)
    old='imm32 P2, 0x027fd9e0\n[P2] = R0;'
    assert text.count(old)==1
    text=text.replace(old,'''imm32 P2, 0x018c2184
P2 = [P2];
imm32 P3, 0x027fc720
P2 = P3 + (P2 << 2);
[P2] = R0;''')
    old='P5 = P2;\nP0 = 9536;'
    assert text.count(old)==1
    text=text.replace(old,'''imm32 P1, 0x018c217c
P3 = 4;
LSETUP(index_out,index_out_end) LC0 = P3;
index_out: R0 = [P1++];
index_out_end: [P2++] = R0;
imm32 P1, 0x018c2184
P1 = [P1];
imm32 P0, 0x027fc71c
P1 = P0 + (P1 << 2);
P3 = 3;
LSETUP(coefficient_out,coefficient_out_end) LC0 = P3;
coefficient_out: R0 = [P1++];
coefficient_out_end: [P2++] = R0;
P5 = P2;
P0 = 9544;''')
    old='R0.H = bank_args;\nR0.L = bank_args;\nP0 = 5;\nEXCPT 0;\nexit 0'
    assert text.count(old)==1
    text=text.replace(old,'R0.H = table_args;\nR0.L = table_args;\nP0 = 5;\nEXCPT 0;\n'+old)
    return text.replace('note_args: .long','table_args: .long 1,0x027fc720,9600\nnote_args: .long',1)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    raw=a.ldr.read_bytes();mapper=module('ldr_map',ROOT/'scripts/map-op1-ldr.py');meta,_=mapper.inspect(raw)
    start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    base=(ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin').read_bytes()
    if len(base)!=512*9316:raise ValueError('Incomplete render fixture')
    changes,expanded=activation_inputs(base);out.mkdir(parents=True);artifacts={}
    probes=[('index',index_inputs(),(ROOT/'research/blackfin/voice-tuning-index.s.in').read_text(),INDEX_CASES*16,'voice246-tuning-index',None),
            ('activation',expanded,activation_template(),512*9272,'drwave246-tuning-index',changes)]
    for name,data,text,result_bytes,prefix,overrides in probes:
        work=out/name;work.mkdir();(work/'sdram.bin').write_bytes(image);(work/'inputs.bin').write_bytes(data)
        (work/'probe.s').write_text(text.replace('@WORK@',work.as_posix()).replace('@BYTES@',str(result_bytes)))
        def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=180,**kw)
        run([a.assembler,'-I',a.testutils,'-o',work/'probe.o',work/'probe.s'])
        run([a.linker,'-T',ROOT/'research/blackfin/voice-contract.ld','-o',work/'probe.elf',work/'probe.o'])
        with (work/'output.bin').open('wb') as stdout,(work/'probe.log').open('wb') as stderr:
            run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',work/'probe.elf'],stdout=stdout,stderr=stderr)
        output=(work/'output.bin').read_bytes()
        if len(output)!=result_bytes+(512+327680+9600 if name=='activation' else 0):raise RuntimeError('Incomplete tuning oracle: '+name)
        if name=='activation' and output[512:512+163840]!=output[-163840:]:raise RuntimeError('Changed base banks')
        if overrides is not None:(work/'overrides.bin').write_bytes(overrides)
        for suffix,path in [('input' if overrides is None else 'overrides',work/('inputs.bin' if overrides is None else 'overrides.bin')),('output',work/'output.bin')]:
            fixture=ROOT/'Tests/ReferenceVectors'/f'{prefix}-{suffix}.bin'
            if a.record:fixture.write_bytes(path.read_bytes())
            if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Tuning oracle differs: '+fixture.name)
            artifacts[fixture.name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'setter':'0x01a842f0','setter_end_exclusive':'0x01a8430e',
        'setter_sha256':hashlib.sha256(image[0x01a842f0-start:0x01a8430e-start]).hexdigest(),
        'stress_cases':65536,'coarse_fine_cartesian_cases':23*199,'continuous_setter_calls':256,
        'activation_records':512,'activation_independent_cases':256,'activation_streams':4,'calls_per_stream':64,'audio_samples':61568,
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
        'limitations':['Original index setter and activation/render consumers only; upstream control objects reviewed statically, not executed.',
            'Test coefficient table starts at zero and receives explicit tuning words at computed indices. No original coefficient calibration recovered.',
            'Prepared register pitch remains held by the harness in the render buffer; original writer, cadence, allocator, ADSR/output and plugin integration remain pending.',
            'Full signed32 stress is isolated to the setter; activation only uses bounded indices 1..2399.',
            'GNU BF524 with documented deferred D-register patch, not hardware/latency validation. Firmware, instructions and ELF stay private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: tuning-index setter and indexed Dr. Wave activation/render')
if __name__=='__main__':main()
