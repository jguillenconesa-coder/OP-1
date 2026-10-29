#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast the successful VoiceCtl control decoder path and its complete table.
The message and table images are explicit. This does not run
the partially encrypted original register-to-buffer handoff.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,RESULT=32768+256,544
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
    compact=bytearray();expanded=bytearray();seed=0x4343494e
    for case in range(CASES):
        seed=(seed*1664525+1013904223)&0xffffffff
        message=bytes([(case*37)&255,(case//256 if case<32768 else case%128),case&255,(case*13)&255])
        row=struct.pack('<I',int(case<=32768))+message+struct.pack('<II',seed,seed^0xabcdef01)
        compact+=row;words=[];state=seed
        for i in range(132):
            state=(state*1664525+1013904223)&0xffffffff;words.append(state)
        expanded+=row+struct.pack('<132I',*words)
    return compact,expanded
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    raw=a.ldr.read_bytes();meta,_=m.inspect(raw);start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700);changes,expanded=inputs()
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'voice-control-input.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=CASES*RESULT:raise RuntimeError('Incomplete output')
    artifacts={}
    for name,path in [('voice246-control-input.bin',out/'overrides.bin'),('voice246-control-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'routine':'0x01a84f8c','routine_end_exclusive':'0x01a85000','table':'0x01dff138','records':CASES,'independent_cases':32768,'controller_indices':128,'unsigned_values':256,'valid_midi_pairs':16384,'continuous_calls':256,'full_table_words_compared':CASES*128,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Successful decoder path only. Original invalid-controller/length assert paths and status/channel dispatcher are not executed.','Source value is unsigned byte, no seven-bit masking. Values above 127 are stress inputs, not valid MIDI data.','No boot, original scheduler, buffer producer, mixing or plugin integration is established.']}
    report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'Source/Voice246ControlInput.h',ROOT/'Tests/Voice246ControlInputTests.cpp',templates/'voice-control-input.s.in',templates/'voice-control-input.ld',Path(__file__).resolve()]}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: 32768 independent control pairs and 256 continuous calls; full table, guards and packet')
if __name__=='__main__':main()
