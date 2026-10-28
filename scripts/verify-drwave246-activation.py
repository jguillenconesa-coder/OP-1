#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast original VoiceCtl activation followed by Dr. Wave's full render.
Tuning and the held prepared pitch are explicit caller inputs, not calibration
or a reconstruction of the original per-sample pitch/ADSR/scheduling route.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES,BASE_RECORD,RECORD,RESULT,BANK=512,9316,9536,9244,163840
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
def inputs(base):
    routing=module('drwave_routing',ROOT/'scripts/verify-drwave246-routing.py')
    prior,expanded=routing.inputs(base)
    seed=0x44524143
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    changes=bytearray();records=bytearray()
    for case in range(CASES):
        row=bytearray(prior[case*148:(case+1)*148])
        action,voice,counter,loaded,selected=struct.unpack_from('<5I',row)
        action&=3 # Every record calls VoiceCtl, without a second note-on/release.
        if case<256:selected=loaded # Every lookup note 0..127 is accepted.
        struct.pack_into('<5I',row,0,action,voice,counter,loaded,selected)
        index=case//2%128
        explicit=0 if case%2==0 else (1,0x7fffffff,0x80000000,0xffffffff,rnd() or 1)[case//2%5]
        if explicit and case%16==1:index=0xffffffff # Ignored when explicit pitch is nonzero.
        velocity=(rnd()&0xffff0000)|(0,1,127,128,255,32767,32768,65535)[case//2%8]
        tuning=0x40000000 if case<256 else (0,1,0x40000000,0x7fffffff,0x80000000,0xffffffff,rnd())[case%7]
        if case>=256 and case%32==1:explicit=tuning=0x80000000
        tail=struct.pack('<5I16I',index,velocity,rnd(),explicit,tuning,*[rnd() for _ in range(16)])
        changes+=row+tail
        record=bytearray(expanded[case*9452:(case+1)*9452])
        struct.pack_into('<3I',record,0,action,voice,counter);struct.pack_into('<I',record,9320,selected)
        records+=record+tail
    if len(changes)!=CASES*232 or len(records)!=CASES*RECORD:raise RuntimeError('Invalid activation dimensions')
    return changes,records

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');args=p.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    basepath=ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin';base=basepath.read_bytes()
    if len(base)!=CASES*BASE_RECORD:raise ValueError('Invalid source fixture')
    mapper=module('ldr_map',ROOT/'scripts/map-op1-ldr.py');raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw)
    start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    if struct.unpack_from('<I',image,0x018c2184-start)[0]!=1200:raise RuntimeError('Unexpected tuning index')
    out.mkdir(parents=True);changes,expanded=inputs(base)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-activation.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=180,**kw)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s']);run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=512+BANK*2+CASES*RESULT or output[512:512+BANK]!=output[-BANK:]:raise RuntimeError('Incomplete output or changed base banks')
    if output[:512]!=image[0x018c21b0-start:0x018c23b0-start]:raise RuntimeError('Original note-table copy differs')
    counts={'accepted':0,'rejected':0,'accepted_explicit_pitch':0,'ignored_large_note_index':0,'factory':0,'updated':0,'read_only':0,'inactive':0}
    used=set()
    for case in range(CASES):
        action,voice,counter,loaded,selected=struct.unpack_from('<5I',changes,case*232)
        index,_,_,explicit=struct.unpack_from('<4I',changes,case*232+148)
        enabled=struct.unpack_from('<I',expanded,case*RECORD+12)[0]
        accepted=loaded==selected;counts['accepted' if accepted else 'rejected']+=1
        counts['accepted_explicit_pitch']+=int(accepted and explicit!=0);counts['ignored_large_note_index']+=int(accepted and explicit!=0 and index>=128)
        counts['factory']+=int(bool(action&2))
        if accepted and not explicit:used.add(index)
        counts['inactive' if not enabled else 'updated' if voice==(counter&65535)%6 else 'read_only']+=1
    if used!=set(range(128)):raise RuntimeError('Missing accepted lookup note')
    artifacts={}
    for name,path in [('drwave246-activation-overrides.bin',out/'overrides.bin'),('drwave246-activation-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Activation oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'voice_activation':'0x01a84414','core_note_on':'0x01967854','runtime_drwave_type':2,'render':'0x0197c870',
            'note_table_source':'0x018c21b0','note_table_destination':'0x01dfe530','note_table_words':128,
            'bounded_copy_fragment':{'start':'0x01a86700','end_exclusive':'0x01a8672e','sha256':hashlib.sha256(image[0x01a86700-start:0x01a8672e-start]).hexdigest(),'scope':'Unmodified PC-relative loop with own appended RTS, not full initializer'},
            'tuning_index_address':'0x018c2184','observed_tuning_index':1200,'explicit_tuning_word_address':'0x027fd9e0',
            'records':CASES,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,'counts':counts,'audio_samples':(CASES-counts['inactive'])*128,
            'base_banks_unchanged':True,'register_image_bytes':64,'source_note_bytes':16,'full_pool_bytes':216,'artifacts':artifacts,
            'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
            'limitations':['Tuning word is supplied explicitly at observed index 1200; no recovered original/default calibration.',
                           'Harness holds the prepared per-voice pitch word in all 128 source positions for the following render, including prior pitch after rejected activation. Not the original pitch provider.',
                           'Events, counter and one rendered voice per record are explicit test scheduling. Common ADSR handoff, allocator, clock, output and plugin integration remain pending.',
                           'Original activation has no defined return value; C++ bool only indicates adapter acceptance. Invalid-input guards are not original assert emulation.',
                           'Valid vtable tags are supplied on loading synthetic pools; no firmware, instructions or private ELF distributed.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original VoiceCtl/Dr. Wave activation',counts)
if __name__=='__main__':main()
