#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Valid rev246 adjustment setter, separately and before the note constructor.
No assertion helper, voice activation/removal, renderer, MIDI caller or L1 runs.
"""
import argparse
import hashlib
import importlib.util
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STANDALONE, INDEPENDENT, STREAMS, CALLS = 72, 576, 4, 64
CASES = STANDALONE+INDEPENDENT+STREAMS*CALLS
# Table now persists; in the chain, only original setters update its eight words.
REGIONS = [(0x01de4c3c,12,False), (0xff8016d0,12,False),
           (0x01dfe4d8,48,True), (0xff801034,776,True),
           (0xff8000d0,160,True), (0x01dfe72c,2576,True),
           (0xff802bc8,8,True)]
IMAGE = sum(n for _,n,_ in REGIONS)
RECORD, SNAPSHOT = 128+IMAGE, 64+IMAGE
RESULT = 4+SNAPSHOT*2

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def u32(data,offset=0):
    return struct.unpack_from('<I',data,offset)[0]

def s32(word):
    return word if word<0x80000000 else word-0x100000000

def put(data,offset,word):
    struct.pack_into('<I',data,offset,word&0xffffffff)

def inputs():
    seed = 0x41444a53
    def rnd():
        nonlocal seed
        seed = (seed*1664525+1013904223)&0xffffffff
        return seed
    edges = [0,1,52,53,74,75,127,128,255,256,32767,32768,65535,65536,
             0x7fffffca,0x7fffffcb,0x7fffffff,0x80000000,0x80000001,
             0xffffffca,0xffffffcb,0xffffffff]
    edges += [1<<i for i in range(32)]+[0xffffffff^(1<<i) for i in range(32)]
    discriminators = [1,0,1,2,1,0x10001,1,0x80000001]
    data = bytearray()
    for c in range(CASES):
        chain = c>=STANDALONE
        k = c-STANDALONE if chain else c
        retained = c>=STANDALONE+INDEPENDENT
        if retained:
            stream,step = divmod(c-STANDALONE-INDEPENDENT,CALLS)
            slot = (step//2+stream)%8
            value = (step//16+stream)%9-4
            discriminator = 1 if step%3==0 else [0,2,0x10001,0xffffffff][(step//3)%4]
            selected = slot if step%2==0 else (slot+3)%8
            reload = int(step==0)
        else:
            slot,value = k%8,(k//8)%9-4
            group = k//72
            discriminator = discriminators[group%8]
            selected = slot if group in (0,1,4,5) else (slot+(1 if group<4 else 3))%8
            reload = 1
        if discriminator==1 and k%5==0:
            selected = [8,0xffffffff,0x10000000][(k//5)%3]
        note = edges[k%len(edges)] if not retained else rnd()
        velocity = edges[(k*7)%len(edges)] if not retained else rnd()
        initial = [((k+i*2)%9)-4 for i in range(8)]
        header = struct.pack('<16I',reload,slot,value&0xffffffff,note,velocity,
                             (k%4)*4,*[v&0xffffffff for v in initial],int(chain),rnd())
        area = struct.pack('<16I',*[rnd() for _ in range(16)])
        image = bytearray(struct.pack('<%dI'%(IMAGE//4),*[rnd() for _ in range(IMAGE//4)]))
        put(image,4,discriminator)
        put(image,16,selected)
        # Poison, not adjustments: chain reloads replace ALL words via setters.
        for i in range(8):
            put(image,32+i*4,0x6bad0000|(rnd()&0xffff))
        data += header+area+image
    if len(data)!=CASES*RECORD:
        raise RuntimeError('Wrong adjustment input dimensions')
    return data

def caller():
    load,save,offset = [],[],0
    for i,(address,n,retained) in enumerate(REGIONS):
        if retained:
            load += ['R0 = [P4];','CC = BITTST(R0,0);',f'IF !CC JUMP retained_{i};']
        load += [f'P0 = {128+offset};','P1 = P4 + P0;',f'imm32 P2, {address}',
                 f'P3 = {n//4};',f'LSETUP(load_{i},load_end_{i}) LC0 = P3;',
                 f'load_{i}: R0 = [P1++];',f'load_end_{i}: [P2++] = R0;']
        if retained:
            load += [f'retained_{i}:']
        save += [f'imm32 P1, {address}',f'P3 = {n//4};',
                 f'LSETUP(save_{i},save_end_{i}) LC0 = P3;',
                 f'save_{i}: R0 = [P1++];',f'save_end_{i}: [P2++] = R0;']
        offset += n
    return '\n'.join(load),'\n'.join(save)

def observations(raw,oracle):
    counts = dict(standalone=0,constructors=0,setter_calls=0,initializing_setters=0,
                  bypass=0,lookup=0,lookup_updated_slot=0,lookup_other_slot=0,
                  ignored_invalid_selected=0,identical_target_writes=0)
    offsets,offset = [],0
    for _,n,_ in REGIONS:
        offsets.append(offset)
        offset += n
    area,shared = bytearray(64),bytearray(IMAGE)
    for c in range(CASES):
        x = raw[c*RECORD:(c+1)*RECORD]
        e = oracle[c*RESULT:(c+1)*RESULT]
        reload,slot,value,_,_,shift = struct.unpack_from('<6I',x)
        chain = u32(x,56)
        if not (0<=slot<8 and -4<=s32(value)<=4):
            raise RuntimeError('Input would enter excluded original error route')
        if reload:
            area[:] = x[64:128]
        for i,(_,n,retained) in enumerate(REGIONS):
            if reload or not retained:
                off = offsets[i]
                shared[off:off+n] = x[128+off:128+off+n]
        if reload and chain:
            for i in range(8):
                initial = u32(x,24+i*4)
                if not -4<=s32(initial)<=4:
                    raise RuntimeError('Invalid original initializing setter input')
                put(shared,32+i*4,initial)
            counts['initializing_setters'] += 8
        counts['setter_calls'] += 1
        counts['identical_target_writes'] += u32(shared,32+slot*4)==value
        put(shared,32+slot*4,value)
        if u32(e)!=4 or e[4:68]!=area or e[68:4+SNAPSHOT]!=shared:
            raise RuntimeError('Original setter changed wrong word, neighbors, arguments or shared state')
        after = e[4+SNAPSHOT:]
        if after[64:]!=shared:
            raise RuntimeError('Constructor changed adjustment table or shared state')
        if chain:
            counts['constructors'] += 1
            discriminator,selected = u32(shared,4),u32(shared,16)
            if discriminator==1:
                counts['bypass'] += 1
                counts['ignored_invalid_selected'] += selected>=8
            else:
                if selected>=8:
                    raise RuntimeError('Invalid selected slot in constructor lookup')
                counts['lookup'] += 1
                counts['lookup_updated_slot' if selected==slot else 'lookup_other_slot'] += 1
            dest = 16+shift
            if (after[:dest]!=area[:dest] or after[dest+16:64]!=area[dest+16:64]
                    or after[dest+9:dest+12]!=area[dest+9:dest+12]):
                raise RuntimeError('Constructor changed argument padding or guards')
        else:
            counts['standalone'] += 1
            if after[:64]!=area:
                raise RuntimeError('Standalone setter changed destination')
        area[:] = after[:64]
    counts['setter_calls'] += counts['initializing_setters']
    return counts

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true')
    a = p.parse_args()
    out = a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):
        raise ValueError('Use a NEW private directory outside the checkout')
    expected = json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:
        raise ValueError('Only inventoried rev246 LDR is accepted')
    spec = importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py')
    mapper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mapper)
    raw = a.ldr.read_bytes()
    meta,_ = mapper.inspect(raw)
    base = int(meta['sdram_start'],16)
    image = bytearray(int(meta['sdram_end'],16)-base)
    cursor = 0
    while cursor<len(raw):
        flags,address,n,arg = struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n] = ((struct.pack('<I',arg)*((n+3)//4))[:n]
                if flags&0x100 else raw[cursor+16:cursor+16+n])
        cursor += 16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700)
    (out/'sdram.bin').write_bytes(image)
    (out/'inputs.bin').write_bytes(inputs())
    load,save = caller()
    arg = '\n'.join(['P1.H = argument_area;','P1.L = argument_area;','P3 = 16;',
                     'LSETUP(arg_save,arg_save_end) LC0 = P3;',
                     'arg_save: R0 = [P1++];','arg_save_end: [P2++] = R0;'])
    initialize = '\n'.join(f'R0 = {i};\nR1 = [P4+{24+i*4}];\nimm32 P0, 0x01a8633c\nCALL (P0);' for i in range(8))
    templates = ROOT/'research/blackfin'
    source = (templates/'voice-note-adjustment.s.in').read_text()
    values = {'WORK':out.as_posix(),'LOAD':load,'INITIALIZE':initialize,
              'ARG_BEFORE':arg.replace('arg_save','arg_before'),'ARG_AFTER':arg.replace('arg_save','arg_after'),
              'SAVE_BEFORE':save.replace('save_','before_'),'SAVE_AFTER':save.replace('save_','after_'),
              'AFTER':str(4+SNAPSHOT),'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}
    for key,value in values.items():
        source = source.replace('@'+key+'@',value)
    (out/'probe.s').write_text(source)
    def run(command,**kwargs):
        subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
    run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
    with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
    if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):
        raise RuntimeError('Use GNU model with both documented writeback patches')
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region',
             '0xff800000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=CASES*RESULT:
        raise RuntimeError('Incomplete setter/constructor output')
    counts = observations((out/'inputs.bin').read_bytes(),(out/'output.bin').read_bytes())
    artifacts = {}
    for name,path in [('voice246-note-adjustment-input.bin',out/'inputs.bin'),
                      ('voice246-note-adjustment-output.bin',out/'output.bin')]:
        target = ROOT/'Tests/ReferenceVectors'/name
        if a.record:
            target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():
            raise RuntimeError('Oracle differs: '+name)
        artifacts[name] = {'bytes':path.stat().st_size,'sha256':sha(path)}
    paths = [Path(__file__).resolve(),templates/'voice-note-adjustment.s.in',
             ROOT/'Source/Voice246NoteAdjustment.h',ROOT/'Source/Voice246NoteArgument.h',
             ROOT/'Tests/Voice246NoteAdjustmentTests.cpp']
    report = {'ldr_sha256':expected,'setter':'0x01a8633c','code_end_exclusive':'0x01a863a6',
              'next_entry':'0x01a863a8','padding_bytes':2,
              'setter_sha256':hashlib.sha256(image[0xa8633c:0xa863a6]).hexdigest(),
              'constructor':'0x01a85014','constructor_end_exclusive':'0x01a85058',
              'records':CASES,'standalone_cases':STANDALONE,'independent_chain_cases':INDEPENDENT,
              'streams':STREAMS,'calls_per_stream':CALLS,'coverage':counts,
              'guarded_destination_bytes':64,'shared_image_bytes':IMAGE,
              'input_record_bytes':RECORD,'output_record_bytes':RESULT,
              'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':r} for addr,n,r in REGIONS],
              'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
              'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},
              'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in paths},
              'model':'GNU BF524 with both documented writeback patches; not hardware timing',
              'limitations':['Only the valid signed32 slot 0..7, adjustment -4..4 route executes. Original error helper and continuations are excluded, not patched or treated as rejection.',
                             'Chain table words are initialized through eight original setters at case/stream start; later changes use setters only. Poison table preload is not a constructor coefficient override.',
                             'Discriminator, selected slot, received note/velocity, argument destination and call order remain explicit. No commercial identity or defaults are inferred.',
                             'No activation, removal, renderer, dispatcher, allocator, scheduler or L1 execution. Shared images are preserved raw stress data, not initialized voices.',
                             'Firmware, original instructions, disassembly and ELF stay private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS original adjustment setter/constructor:',counts)

if __name__=='__main__':
    main()
