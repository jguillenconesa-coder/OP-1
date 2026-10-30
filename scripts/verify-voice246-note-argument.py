#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Execute the complete rev246 note-argument constructor in a private GNU oracle.
Raw arguments and global context are explicit; no MIDI dispatcher or voice route.
"""
import argparse
import hashlib
import importlib.util
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CASES, INDEPENDENT, STREAMS, CALLS = 1280, 1024, 4, 64
# Address, byte count, retained across continuous calls. Read-only context is
# deliberately raw stress data, not a claim of initialized/valid voice objects.
REGIONS = [(0x01de4c3c,12,False), (0xff8016d0,12,False),
           (0x01dfe4d8,48,False), (0xff801034,776,True),
           (0xff8000d0,160,True), (0x01dfe72c,2576,True),
           (0xff802bc8,8,True)]
IMAGE = sum(n for _,n,_ in REGIONS)
RECORD, RESULT = 80+IMAGE, 64+IMAGE

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inputs():
    seed = 0x4e415247
    def rnd():
        nonlocal seed
        seed = (seed*1664525+1013904223) & 0xffffffff
        return seed
    edges = [0,1,52,53,74,75,127,128,255,256,32767,32768,65535,65536,
             0x7fffffca,0x7fffffcb,0x7fffffff,0x80000000,0x80000001,
             0xffffffca,0xffffffcb,0xffffffff]
    edges += [1<<i for i in range(32)] + [0xffffffff^(1<<i) for i in range(32)]
    discriminators = [1,0,2,0xffffffff,0x10001,0x80000001,0x1000001,0xffffff01]
    adjustments = [0,1,2,3,4,0xffffffff,0xfffffffe,0xfffffffd,0xfffffffc,
                   0x7fffffff,0x80000000,0x15555555,0x15555556,
                   0x2aaaaaaa,0x2aaaaaab,0x55555555,0xaaaaaaaa]
    data = bytearray()
    for c in range(CASES):
        note = edges[(c//8)%len(edges)] if c<INDEPENDENT else rnd()
        velocity = edges[(c//8*7+c%8)%len(edges)] if c<INDEPENDENT else rnd()
        discriminator = discriminators[c%8]
        slot = (c//8)%8
        if discriminator==1:
            slot = [0,7,8,0xffffffff,0x10000000,0x80000000][(c//8)%6]
        area = bytearray(struct.pack('<16I',*[rnd() for _ in range(16)]))
        image = bytearray(struct.pack('<%dI'%(IMAGE//4),*[rnd() for _ in range(IMAGE//4)]))
        struct.pack_into('<I',image,4,discriminator)
        struct.pack_into('<I',image,16,slot)
        for i in range(8):
            word = adjustments[(c//8+i)%len(adjustments)] if c<INDEPENDENT else rnd()
            struct.pack_into('<I',image,32+i*4,word)
        # Some selected words are independent of their neighbors, with raw high bits.
        if discriminator!=1 and c>=768:
            struct.pack_into('<I',image,32+slot*4,rnd())
        reload = int(c<INDEPENDENT or (c-INDEPENDENT)%CALLS==0)
        data += struct.pack('<4I',reload,note,velocity,(c%4)*4) + area + image
    if len(data)!=CASES*RECORD:
        raise RuntimeError('Incorrect input dimensions')
    return data

def caller():
    load, save, offset = [], [], 0
    for i,(address,n,retained) in enumerate(REGIONS):
        if retained:
            load += ['R0 = [P4];','CC = BITTST(R0,0);',f'IF !CC JUMP retained_{i};']
        load += [f'P0 = {80+offset};','P1 = P4 + P0;',f'imm32 P2, {address}',
                 f'P3 = {n//4};',f'LSETUP(load_{i},load_end_{i}) LC0 = P3;',
                 f'load_{i}: R0 = [P1++];',f'load_end_{i}: [P2++] = R0;']
        if retained:
            load += [f'retained_{i}:']
        save += [f'imm32 P1, {address}',f'P3 = {n//4};',
                 f'LSETUP(save_{i},save_end_{i}) LC0 = P3;',
                 f'save_{i}: R0 = [P1++];',f'save_end_{i}: [P2++] = R0;']
        offset += n
    return '\n'.join(load), '\n'.join(save)

def observations(input_bytes, output_bytes):
    counts = dict(bypass=0,table_lookup=0,invalid_index_ignored=0,note_add_wrap=0,
                  table_product_wrap=0,nonzero_preserved_padding=0)
    offsets = []
    for _,n,_ in REGIONS:
        offsets.append(sum(x[1] for x in REGIONS[:len(offsets)]))
    area,shared = bytearray(64),bytearray(IMAGE)
    for c in range(CASES):
        x = input_bytes[c*RECORD:(c+1)*RECORD]
        e = output_bytes[c*RESULT:(c+1)*RESULT]
        reload,note,velocity,shift = struct.unpack_from('<4I',x)
        if reload:
            area[:] = x[16:80]
        for i,(_,n,retained) in enumerate(REGIONS):
            if reload or not retained:
                off = offsets[i]
                shared[off:off+n] = x[80+off:80+off+n]
        dest = 16+shift
        discriminator = struct.unpack_from('<I',shared,4)[0]
        slot = struct.unpack_from('<I',shared,16)[0]
        if discriminator==1:
            counts['bypass'] += 1
            counts['invalid_index_ignored'] += slot>=8
        else:
            counts['table_lookup'] += 1
            word = struct.unpack_from('<I',shared,32+slot*4)[0]
            counts['table_product_wrap'] += word*12>0xffffffff
        counts['note_add_wrap'] += note+53>0xffffffff
        counts['nonzero_preserved_padding'] += any(area[dest+9:dest+12])
        if (e[:dest]!=area[:dest] or e[dest+16:64]!=area[dest+16:64]
                or e[dest+9:dest+12]!=area[dest+9:dest+12] or e[64:]!=shared):
            raise RuntimeError('Original constructor changed padding, guards or shared image')
        if (struct.unpack_from('<I',e,dest+4)[0]!=velocity or e[dest+8]!=1
                or struct.unpack_from('<I',e,dest+12)[0]!=0):
            raise RuntimeError('Original constructor field write contract differs')
        area[:] = e[:64]
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
        raise ValueError('Only the inventoried rev246 LDR is accepted')
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
    templates = ROOT/'research/blackfin'
    source = (templates/'voice-note-argument.s.in').read_text()
    for key,value in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,
                      'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():
        source = source.replace('@'+key+'@',value)
    (out/'probe.s').write_text(source)
    def run(command,**kwargs):
        subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    # The same documented model preflight used by the neighboring voice studies.
    run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
    run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
    with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
    if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):
        raise RuntimeError('Use the GNU model with both documented writeback patches')
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','128M',
             '--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',
             out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=CASES*RESULT:
        raise RuntimeError('Incomplete constructor output')
    counts = observations((out/'inputs.bin').read_bytes(),(out/'output.bin').read_bytes())
    artifacts = {}
    for name,source in [('voice246-note-argument-input.bin',out/'inputs.bin'),
                        ('voice246-note-argument-output.bin',out/'output.bin')]:
        target = ROOT/'Tests/ReferenceVectors'/name
        if a.record:
            target.write_bytes(source.read_bytes())
        if target.read_bytes()!=source.read_bytes():
            raise RuntimeError('Oracle differs: '+name)
        artifacts[name] = {'bytes':source.stat().st_size,'sha256':sha(source)}
    paths = [Path(__file__).resolve(),templates/'voice-note-argument.s.in',
             ROOT/'Source/Voice246NoteArgument.h',ROOT/'Tests/Voice246NoteArgumentTests.cpp']
    report = {'ldr_sha256':expected,'routine':'0x01a85014','end_exclusive':'0x01a85058',
              'routine_sha256':hashlib.sha256(image[0xa85014:0xa85058]).hexdigest(),
              'records':CASES,'independent':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,
              'argument_bytes':16,'guarded_destination_bytes':64,'shared_image_bytes':IMAGE,
              'input_record_bytes':RECORD,'output_record_bytes':RESULT,'coverage':counts,
              'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':r} for addr,n,r in REGIONS],
              'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
              'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},
              'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in paths},
              'model':'GNU BF524 with both documented writeback patches; no hardware timing',
              'limitations':['Complete constructor only; no caller, dispatcher, allocator, activation, release or render execution.',
                             'Eight table words and globals are explicit; no default value or commercial mode identity is inferred.',
                             'Large table words and raw 32-bit notes/velocities are stress inputs, not asserted normal musical ranges.',
                             'Shared images are raw preserved stress data, not initialized voice state. No L1 execution, pitch-buffer producer or cadence is reconstructed.',
                             'Original firmware, instructions, disassembly and ELF remain private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS original note-argument constructor:',CASES,'records;',IMAGE,'shared bytes per call')

if __name__=='__main__':
    main()
