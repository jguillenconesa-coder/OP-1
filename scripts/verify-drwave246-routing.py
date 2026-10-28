#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Run original Core factory/note-on/release and explicitly scheduled Dr. Wave.
Only numeric observations and own callers are published, not original code.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CASES,BASE_RECORD,RECORD,RESULT,BANK=512,9316,9452,9164,163840
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs(base):
    seed=0x44525254
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    changes=bytearray();expanded=bytearray()
    edges=(0,1,2,3,4,5,32767,32768,65535)
    for case in range(CASES):
        independent=case<256;block=(case-256)%64;stream=max(0,(case-256)//64)
        action=int(independent or block==0)
        action|=2 if case%11==0 or (not independent and block==0) else 0
        action|=4 if (independent and case%4!=0) or (not independent and block%3==0) else 0
        action|=8 if case%5==0 or (not independent and block%11==7) else 0
        voice=case%6 if independent else block%6 if stream%2==0 else 5-block%6
        loaded=(case//6)%8;selected=loaded if case%3 else (loaded+1)%8
        old=struct.unpack_from('<I',base,case*BASE_RECORD+8)[0]
        word=edges[(case//6)%len(edges)] if independent else ((32760,65528,5,100)[stream]+block//6)&65535
        counter=(old&0xffff0000)|word
        sounds=[]
        for sound in range(8):
            sounds.extend([rnd()%24576,rnd()%32768,rnd()%32768,rnd()%32768,
                           *[rnd()%65536-32768 for _ in range(4)]])
        changes+=struct.pack('<5I64h',action,voice,counter,loaded,selected,*sounds)
        record=bytearray(base[case*BASE_RECORD:(case+1)*BASE_RECORD])
        struct.pack_into('<3I',record,0,action,voice,counter)
        # Core's indirect calls require valid Dr. Wave vtables on loaded objects.
        for slot in range(6):struct.pack_into('<I',record,536+slot*36,0x0190fb2c)
        expanded+=record+struct.pack('<2I64h',loaded,selected,*sounds)
    assert len(changes)==CASES*148 and len(expanded)==CASES*RECORD
    return changes,expanded

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');args=p.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    basepath=ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin';base=basepath.read_bytes()
    if len(base)!=CASES*BASE_RECORD:raise ValueError('Invalid source fixture')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw);start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);changes,expanded=inputs(base)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-routing.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=180,**kw)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s']);run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=BANK*2+CASES*RESULT or output[:BANK]!=output[-BANK:]:raise RuntimeError('Incomplete output or changed base banks')
    counts={'factory':0,'accepted_note':0,'rejected_note':0,'release':0,'release_sound_mismatch':0,'updated':0,'read_only':0,'inactive':0}
    for case in range(CASES):
        action,voice,counter,loaded,selected=struct.unpack_from('<5I',changes,case*148)
        enabled=struct.unpack_from('<I',expanded,case*RECORD+12)[0]
        counts['factory']+=int(bool(action&2));counts['release']+=int(bool(action&8))
        counts['release_sound_mismatch']+=int(bool(action&8) and loaded!=selected)
        if action&4:
            accepted=int(loaded==selected);counts['accepted_note' if accepted else 'rejected_note']+=1
            if struct.unpack_from('<I',output,BANK+case*RESULT+8784)[0]!=accepted:raise RuntimeError('Unexpected Core note-on result')
        counts['inactive' if not enabled else 'updated' if voice==(counter&65535)%6 else 'read_only']+=1
    artifacts={}
    for name,path in [('drwave246-routing-overrides.bin',out/'overrides.bin'),('drwave246-routing-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Routing oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'core_factory':'0x01967960','core_note_on':'0x01967854','core_release':'0x019678f0',
            'runtime_drwave_type':2,'voice_constructor':'0x0197ada8','voice_reset':'0x0197a826','voice_release':'0x0197a838','render':'0x0197c870',
            'producer_pointers':'0x01aaad18','loaded_sound':'0x01aaac0c','selected_sound':'0xff8016d4','sound_controls':'0xff8013c4',
            'records':CASES,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,'counts':counts,'audio_samples':(CASES-counts['inactive'])*128,
            'base_banks_unchanged':True,'full_pool_bytes':216,'pre_render_pool_bytes':216,'additional_routing_bytes':164,
            'artifacts':artifacts,'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
            'limitations':['Core callbacks and Dr. Wave render run unmodified, but event order, one rendered voice per call, counter and pitch are explicit harness inputs.',
                           'Only valid six voice/eight sound indices are sent to the original routines. C++ guards do not emulate original assertions.',
                           'Valid Dr. Wave vtable tags are explicitly supplied when loading synthetic object pools. This is not recovered original allocation.',
                           'Common ADSR, original counter provider/cadence, tuning/pitch provider, allocator, output conversion and plugin integration remain pending.',
                           'No firmware, instructions, private ELF, presets or installer are published by this delta.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original Dr. Wave Core routing',counts)
if __name__=='__main__':main()
