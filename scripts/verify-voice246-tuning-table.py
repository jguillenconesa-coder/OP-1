#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Execute the original tuning-table tail and consume its generated coefficients
in indexed VoiceCtl/Dr. Wave activation. Module/event initialization is skipped
with an own frame adapter; the original tail/math helpers stay unmodified.
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
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def activation_inputs(base):
    prior=module('tuning_index',ROOT/'scripts/verify-voice246-tuning-index.py');changes,data=prior.activation_inputs(base)
    changes=bytearray(changes);data=bytearray(data)
    for case in range(512):
        # Poison the obsolete explicit tuning field to detect accidental reuse.
        struct.pack_into('<I',changes,case*240+164,0xdeadbeef)
        struct.pack_into('<I',data,case*9544+9468,0xdeadbeef)
    return changes,data
def activation_template():
    prior=module('tuning_index',ROOT/'scripts/verify-voice246-tuning-index.py');text=prior.activation_template()
    begin=text.index('// Explicit zero-filled test coefficient table')
    end=text.index('imm32 P1, 0x018c217c',begin)
    text=text[:begin]+'CALL build_tuning_table;\n'+text[end:]
    old='''P0 = 9468;
P1 = P4 + P0;
R0 = [P1];
imm32 P2, 0x018c2184
P2 = [P2];
imm32 P3, 0x027fc720
P2 = P3 + (P2 << 2);
[P2] = R0;'''
    if text.count(old)!=1:raise RuntimeError('Unexpected prior explicit-tuning harness')
    text=text.replace(old,'// Generated original table is retained; no per-record coefficient override.')
    wrapper='''build_tuning_table:
LINK 0;
[--SP] = (R7:4, P5:3);
SP += -16;
imm32 P0, 0x01a85454
JUMP (P0);
'''
    if text.count('\n.data\n')!=1:raise RuntimeError('Unexpected activation template layout')
    return text.replace('imm32 SP, 0x03000000','imm32 SP, 0x03000000\nSP += -64;',1).replace('\n.data\n','\n'+wrapper+'.data\n',1)
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
    constants=struct.unpack_from('<3d',image,0x01909764-start)
    if constants!=(1200.0,2.0,2.0):raise RuntimeError('Unexpected producer constants')
    base=(ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin').read_bytes()
    if len(base)!=512*9316:raise ValueError('Incomplete render fixture')
    changes,data=activation_inputs(base);out.mkdir(parents=True);artifacts={};table=None
    probes=[('table',(ROOT/'research/blackfin/voice-tuning-table.s.in').read_text(),None,28872,'voice246-tuning-table'),
            ('activation',activation_template(),data,512+327680+512*9272+9600,'drwave246-tuning-table')]
    for name,text,inputs,size,prefix in probes:
        work=out/name;work.mkdir();(work/'sdram.bin').write_bytes(image)
        if inputs is not None:(work/'inputs.bin').write_bytes(inputs);(work/'overrides.bin').write_bytes(changes)
        (work/'probe.s').write_text(text.replace('@WORK@',work.as_posix()).replace('@BYTES@',str(512*9272)))
        def run(args,**kw):subprocess.run([str(x) for x in args],check=True,timeout=180,**kw)
        run([a.assembler,'-I',a.testutils,'-o',work/'probe.o',work/'probe.s'])
        run([a.linker,'-T',ROOT/'research/blackfin/voice-contract.ld','-o',work/'probe.elf',work/'probe.o'])
        with (work/'output.bin').open('wb') as stdout,(work/'probe.log').open('wb') as stderr:
            run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',work/'probe.elf'],stdout=stdout,stderr=stderr)
        output=(work/'output.bin').read_bytes()
        if len(output)!=size:raise RuntimeError('Incomplete table oracle: '+name)
        if name=='table':
            table=output[4:9604]
            for row,seed in enumerate([0,0xa5a5a5a5,0xffffffff]):
                record=output[row*9624:(row+1)*9624]
                if record[4:9604]!=table or record[:4]+record[9604:]!=struct.pack('<6I',*[seed]*6):raise RuntimeError('Changed table guards/index or preload-dependent coefficients')
        elif output[512:512+163840]!=output[-163840:] or output[-163840-9600:-163840]!=table:raise RuntimeError('Changed banks or generated table')
        files=[('output',work/'output.bin')]
        if name=='activation':files.insert(0,('overrides',work/'overrides.bin'))
        for suffix,path in files:
            fixture=ROOT/'Tests/ReferenceVectors'/f'{prefix}-{suffix}.bin'
            if a.record:fixture.write_bytes(path.read_bytes())
            if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Generated tuning oracle differs: '+fixture.name)
            artifacts[fixture.name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'table_tail_entry':'0x01a85454','table_tail_end_exclusive':'0x01a854ca',
        'tail_sha256':hashlib.sha256(image[0x01a85454-start:0x01a854ca-start]).hexdigest(),
        'constants_address':'0x01909764','binary64_constants':list(constants),'table_words':2400,'preload_patterns':[0,0xa5a5a5a5,0xffffffff],
        'activation_records':512,'activation_independent_cases':256,'activation_streams':4,'calls_per_stream':64,'audio_samples':61568,
        'coefficient_examples':{str(i):hex(struct.unpack_from('<I',table,i*4)[0]) for i in [0,1200,2399]},
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
        'limitations':['Own frame adapter enters only the original table-generating tail; module/event setup before it is not executed or reconstructed.',
            'All tail and math-helper instructions execute at original SDRAM addresses without patching. No boot/scheduler validation.',
            'Generated coefficients now feed activation; the register-to-render buffer copy is still explicit held-pitch harness code.',
            'Original pitch writer, cadence, allocator, ADSR/output and plugin integration remain pending. No MIDI/automation/session changes.',
            'GNU BF524 with documented deferred D-register patch, not hardware/latency validation. Original bytes and ELF remain private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: generated original tuning table and Dr. Wave activation/render without coefficient overrides')
if __name__=='__main__':main()
