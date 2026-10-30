#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original rev246 last-valid-voice query in an explicit non-error context.
No error helper, L1, note events, allocator, scheduler or renderer is executed.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
INDEPENDENT,STREAMS,CALLS=512,8,64
CASES=INDEPENDENT+STREAMS*CALLS
# Address, bytes, retained within each stream. Lists are explicit every call;
# the query alone updates the persistent word in the first region.
REGIONS=[(0x01dfe4d0,64,True),(0x01dfe72c,2576,False),
         (0xff801034,776,True),(0xff8000d0,160,True),(0xff802bc8,8,True),
         (0x01b60ef0,128,False),(0xff8016d0,16,False),(0x01de4c3c,12,False)]
IMAGE=sum(n for _,n,_ in REGIONS)
RECORD,RESULT=16+IMAGE,4+IMAGE
LIST,COUNT=72,2632
DISCARDED=[0xffffffff,0xfffffffe,0x80000000,0x80000005,6,7,0x10000,
           0x1000000,0x10000005,0x7fffffff,0x100,0x10001,0xffffff00]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def u32(data,offset=0):return struct.unpack_from('<I',data,offset)[0]
def put(data,offset,value):struct.pack_into('<I',data,offset,value&0xffffffff)

def inputs():
    seed=0x4c565259
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    data=bytearray();paired=None
    for c in range(CASES):
        image=bytearray(struct.pack('<%dI'%(IMAGE//4),*[rnd() for _ in range(IMAGE//4)]))
        if c<INDEPENDENT:
            reload=1;count=c%129;kind=c%8
        else:
            stream,step=divmod(c-INDEPENDENT,CALLS)
            reload=int(step==0);count=0 if step%9==0 else (step*11+stream*17)%128+1;kind=step%8
        if c<64:count=[2,6,17,128][(c//2)%4]
        for i in range(128):
            p=LIST+i*20
            put(image,p,53+i%4) # Duplicated note words, not a voice priority.
            image[p+8]=0 if (i+c)%3==0 else (rnd()&255)
            put(image,p+16,DISCARDED[(c+i)%len(DISCARDED)] if i<count else (i+c+1)%6)
        def voice(i,value):put(image,LIST+i*20+16,value)
        if c<64:
            if c%2==0:
                voice(0,0);voice(count-1,5)
                if count>2:voice(count//2,3)
                paired=bytes(image[LIST:LIST+2560])
            else:
                image[LIST:LIST+2560]=paired
                active=[paired[i*20:(i+1)*20] for i in range(count)]
                image[LIST:LIST+count*20]=b''.join(reversed(active))
        elif count:
            target=(c//8)%6
            if kind==1:voice(0,target)
            elif kind==2:voice(count-1,target)
            elif kind==3:
                voice(0,5)
                if count>2:voice(count//2,3)
                voice(count-1,0)
            elif kind==4:
                for i in range(count):
                    if i%3==0:voice(i,(i+c)%6)
                voice(count//2,target)
            elif kind==5:
                voice(0,target);image[LIST+8]=0
            elif kind==6:
                voice(0,0)
                if count>2:voice(count//2,5)
                voice(count-1,2)
                active=[bytes(image[LIST+i*20:LIST+(i+1)*20]) for i in range(count)]
                image[LIST:LIST+count*20]=b''.join(reversed(active))
        valid=[u32(image,LIST+i*20+16) for i in range(count) if u32(image,LIST+i*20+16)<6]
        previous=DISCARDED[(c//2)%len(DISCARDED)] if c%2==0 and valid else c%6
        if c>=INDEPENDENT and reload:previous=c%6
        put(image,4,previous)
        put(image,COUNT,count)
        data+=struct.pack('<4I',reload,rnd(),rnd(),rnd())+image
    if len(data)!=CASES*RECORD:raise RuntimeError('Wrong query dimensions')
    return data

def observations(raw,oracle=None):
    # Also called BEFORE execution: no record may reach the excluded error path.
    image=bytearray(IMAGE);offsets=[];offset=0
    for _,n,_ in REGIONS:offsets.append(offset);offset+=n
    counts=dict(empty_lists=0,fallback_returns=0,lists_with_valid_id=0,
                valid_id_reads=0,discarded_id_reads=0,repaired_previous=0,
                persistent_changed=0,duplicate_valid_lists=0,zero_byte8_valid_reads=0)
    lengths=set();returned=set();invalid_seen=set()
    for c in range(CASES):
        p=raw[c*RECORD:(c+1)*RECORD]
        for i,(_,n,retain) in enumerate(REGIONS):
            if u32(p) or not retain:
                off=offsets[i];image[off:off+n]=p[16+off:16+off+n]
        count=u32(image,COUNT);prior=u32(image,4);candidate=prior;valid=[]
        if count>128:raise RuntimeError('Count outside supplied original list capacity')
        lengths.add(count);counts['empty_lists']+=count==0
        for i in range(count):
            word=u32(image,LIST+i*20+16)
            if word<6:
                candidate=word;valid.append(word);counts['valid_id_reads']+=1
                counts['zero_byte8_valid_reads']+=image[LIST+i*20+8]==0
            else:counts['discarded_id_reads']+=1;invalid_seen.add(word)
        if candidate>=6:raise RuntimeError('Input would execute excluded original error helper')
        counts['lists_with_valid_id' if valid else 'fallback_returns']+=1
        counts['repaired_previous']+=prior>=6
        counts['persistent_changed']+=prior!=candidate
        counts['duplicate_valid_lists']+=len(valid)>len(set(valid))
        returned.add(candidate);put(image,4,candidate)
        if oracle is not None:
            e=oracle[c*RESULT:(c+1)*RESULT]
            if u32(e)!=candidate or e[4:]!=image:
                raise RuntimeError('Original return/persistent/list/shared image differs at record '+str(c))
    counts.update(list_lengths=len(lengths),returned_voices=sorted(returned),
                  discarded_patterns_seen=len(invalid_seen),reversed_order_pairs=32)
    return counts

def caller():
    load=[];save=[];offset=0
    for i,(address,n,retain) in enumerate(REGIONS):
        if retain:load+=['R0 = [P4];','CC = BITTST(R0,0);',f'IF !CC JUMP retained_{i};']
        load += [f'P0 = {16+offset};','P1 = P4 + P0;',f'imm32 P2, {address}',
                 f'P3 = {n//4};',f'LSETUP(load_{i},load_end_{i}) LC0 = P3;',
                 f'load_{i}: R0 = [P1++];',f'load_end_{i}: [P2++] = R0;']
        if retain:load+=[f'retained_{i}:']
        save += [f'imm32 P1, {address}',f'P3 = {n//4};',
                 f'LSETUP(save_{i},save_end_{i}) LC0 = P3;',
                 f'save_{i}: R0 = [P1++];',f'save_end_{i}: [P2++] = R0;']
        offset+=n
    return '\n'.join(load),'\n'.join(save)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 LDR is accepted')
    raw_inputs=inputs();observations(raw_inputs)
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    raw=a.ldr.read_bytes();meta,_=m.inspect(raw);base=int(meta['sdram_start'],16)
    image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=((struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n])
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(raw_inputs)
    templates=ROOT/'research/blackfin';load,save=caller()
    source=(templates/'voice-last-voice-query.s.in').read_text()
    for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
    (out/'probe.s').write_text(source)
    def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
    run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
    with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
    if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Use both documented GNU writeback patches')
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    oracle=(out/'output.bin').read_bytes()
    if len(oracle)!=CASES*RESULT:raise RuntimeError('Incomplete original query output')
    counts=observations(raw_inputs,oracle);artifacts={}
    for name,path in [('voice246-last-voice-query-input.bin',out/'inputs.bin'),('voice246-last-voice-query-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    paths=[Path(__file__).resolve(),templates/'voice-last-voice-query.s.in',ROOT/'Source/Voice246LastVoiceQuery.h',ROOT/'Tests/Voice246LastVoiceQueryTests.cpp']
    report={'ldr_sha256':expected,'entry':'0x01a863a8','end_exclusive':'0x01a86420','code_bytes':120,
            'routine_sha256':hashlib.sha256(image[0xa863a8:0xa86420]).hexdigest(),
            'records':CASES,'independent_cases':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,
            'coverage':counts,'shared_bytes':IMAGE,'input_record_bytes':RECORD,'output_record_bytes':RESULT,
            'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in REGIONS],
            'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},
            'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},
            'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in paths},
            'model':'GNU BF524 with both documented writeback patches; not hardware timing',
            'limitations':['Original error helper at 0x01919aa8 is excluded: eligibility is checked before execution, requiring a valid final persistent word.',
                           'Lists/counts are explicit every call, capacity 0..128. Persistent word and selected monitor regions carry between stream calls; no original list producer is recovered.',
                           'No note events, constructor, setter, renderer, allocator, MIDI dispatcher, scheduler or L1 execution. No commercial mode identity is inferred.',
                           'Raw shared monitor images are preservation stress data, not initialized voice state. Work registers/DAG state and stack are not a complete machine model.',
                           'Firmware, original instructions, disassembly and ELF remain private.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original last-voice query:',counts)

if __name__=='__main__':main()
