#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Probe the readable Cluster normalization entries and original six-slot factory.
The documented 8-of-24-byte XTEA layout classifies readable words; this does not
recover a key or validate the remaining encrypted words. Private firmware only.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import math
ROOT=Path(__file__).resolve().parents[1]
MASK=0x79

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--record',action='store_true')
    args=parser.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a new private output outside the checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if digest(args.ldr)!=expected:raise ValueError('Unexpected firmware revision/hash')
    raw=args.ldr.read_bytes();offset=0x1fe870;flags,address,count,argument=struct.unpack_from('<4I',raw,offset)
    if (flags,address,count,argument)!=(0xad812406,0xff9003f0,656,0):raise ValueError('Unexpected L1 containing block')
    clear=[];float_words=[0]*8
    for index in range(8):
        relative=0xff900524-address+index*4
        readable=relative%24>=8
        if readable:
            float_words[index]=struct.unpack_from('<I',raw,offset+16+relative)[0]
            clear.append({'count':index+1,'payload_offset':relative,'offset_mod24':relative%24,'float32_bits':hex(float_words[index]),'float32_value':struct.unpack('<f',struct.pack('<I',float_words[index]))[0]})
    if sum(1<<(row['count']-1) for row in clear)!=MASK:raise ValueError('Unexpected partial plaintext mask')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        f,a,n,arg=struct.unpack_from('<4I',raw,cursor)
        if base<=a and a+n<=base+len(image) and not f&0x1000:
            image[a-base:a-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if f&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if f&0x100 else n)
    seed=0x43494e49
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    factory=bytearray(struct.pack('<I8I',MASK,*float_words))
    valid_counts=[row['count'] for row in clear]
    for case in range(60):
        memory=bytes([[0,0xff,0x55,0xaa][case%4]])*648 if case<24 else struct.pack('<162I',*[rnd() for _ in range(162)])
        factory+=memory+struct.pack('<I4hI',case%6,valid_counts[case%5]*2048+1024,0,512,3,rnd())
    if len(factory)!=36+60*664:raise RuntimeError('Unexpected factory input dimensions')
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(factory)
    prefix=''
    for row in clear:
        index=row['count']-1
        prefix+=f'imm32 P0, {0xff900524+index*4}\nimm32 R0, {float_words[index]}\n[P0] = R0;\nimm32 P0, 0x01aa7014\nCALL (P0);\nimm32 P0, {0x1baa91c+index*2}\nW[P0] = R0;\nP0.H = norm_results;\nP0.L = norm_results;\nP0 += {4+index*2};\nW[P0] = R0;\n'
    templates=ROOT/'research/blackfin'
    source=(templates/'cluster-init.s.in').read_text().replace('@NORMALIZATION@',prefix).replace('@WORK@',out.as_posix()).replace('@BYTES@',str(60*764))
    (out/'probe.s').write_text(source)
    def run(command,**kw):subprocess.run([str(value) for value in command],check=True,timeout=120,**kw)
    def simulate(prefix):
        run([args.assembler,'-I',args.testutils,'-o',out/(prefix+'.o'),out/(prefix+'.s')]);run([args.linker,'-T',templates/'voice-contract.ld','-o',out/(prefix+'.elf'),out/(prefix+'.o')])
        with (out/(prefix+'.bin')).open('wb') as stdout,(out/(prefix+'.log')).open('wb') as stderr:
            run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/(prefix+'.elf')],stdout=stdout,stderr=stderr)
    simulate('probe');result=(out/'probe.bin').read_bytes()
    if len(result)!=20+60*764 or struct.unpack_from('<I',result)[0]!=MASK:raise RuntimeError('Incomplete factory oracle')
    coefficients=list(struct.unpack_from('<8h',result,4))
    for row in clear:row['q15']=coefficients[row['count']-1]
    for case in range(60):
        if struct.unpack_from('<I',result,20+case*764)[0]!=0xffb00000+(case%6)*108:raise RuntimeError('Unexpected factory return pointer')
    (out/'factory-output.bin').write_bytes(result)
    # Five streams: only readable normalization counts, original verified damping.
    damping=json.loads((templates/'cluster246-contract.json').read_text())['damping']['verified_q31']
    render=bytearray()
    for count_index,count_value in enumerate(valid_counts):
        for block in range(128):
            state=bytes(108);controls=[count_value*2048+1024,round(block*32767/127),512+(block%24)*1024,3+(block%4)*545]
            pitch=[round(110*2**count_index*2**(.03*math.sin((block*128+j)/600))/88200*2**32) for j in range(128)]
            action=(3 if block==0 else 4 if block==80 else 0)|(8 if block%7==0 else 0)
            render+=state+struct.pack('<4h128I8h32II',*controls,*pitch,*coefficients,*(damping+[0]*8),rnd())+bytes(32)+struct.pack('<I',action)
    (out/'known-inputs.bin').write_bytes(render)
    conversion=''.join(f'imm32 P0, {0x18befb0+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0x106a954+i*4}\n[P0] = R0;\n' for i in range(24))
    source=(templates/'cluster-stream.s.in').read_text().replace('@COEFFICIENTS@',conversion).replace('@WORK@',out.as_posix()).replace('/inputs.bin','/known-inputs.bin').replace('@BYTES@',str(640*400))
    (out/'render.s').write_text(source);simulate('render')
    if (out/'render.bin').stat().st_size!=160+640*400:raise RuntimeError('Incomplete known-normalization render')
    artifacts={}
    for name,path in [('cluster246-init-input.bin',out/'inputs.bin'),('cluster246-init-output.bin',out/'factory-output.bin'),('cluster246-known-input.bin',out/'known-inputs.bin'),('cluster246-known-output.bin',out/'render.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':digest(path)}
    evidence={'ldr_sha256':expected,'known_normalization_mask':hex(MASK),'observed_entries':clear,'unresolved_counts':[2,3,8],'no_key_recovered':True,'no_full_L1_decryption_claimed':True,'layout_model':'Community OP-1 XTEA format: first 8 bytes of each complete 24-byte group encrypted; remaining 16 unchanged. Only unchanged words used.','factory':'0x019607e4','pool_base':'0xffb00000','pool_slots':6,'slot_stride':108,'factory_cases':60,'reset':'0x01960664','scalar_conversion':'0x01aa7014','known_normalization_render_blocks':640,'known_normalization_samples':81920,'artifacts':artifacts,'tools':{str(path):digest(path) for path in [args.assembler,args.linker,args.simulator]},'limitations':['Three normalization counts remain unknown; no inferred coefficients are used.','Factory slots do not implement the complete allocator/cadence; no plugin integration or full instrument equivalence claimed.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: five readable normalization conversions, 60 original factory/reset cases and 81920 samples with known normalization')
if __name__=='__main__':main()
