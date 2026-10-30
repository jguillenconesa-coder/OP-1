#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Bounded research probe: original last-voice query -> address accessor.
The enclosing candidate graphics callback is inspected statically, NOT executed.
No product implementation, UI, audio renderer or public firmware artifact.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BUFFER_START,BUFFER_BYTES=0x010225ec-8,6*0xc000+16
RAW=list(range(8))+[0xffff,0x10000,0x10001,0x7fffffff,0x80000000,0xffffffff,0x01000000,0x10000000]
RAW += [1<<i for i in range(32)]+[0xffffffff^(1<<i) for i in range(32)]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def address(word):return (0x010225ec+word*0xc000)&0xffffffff
def replace_once(source, old, new):
 if source.count(old)!=1:raise RuntimeError('Own baseline harness layout changed')
 return source.replace(old,new)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();out=a.output.resolve()
 if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
 expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
 if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 LDR is supported')
 q=module('query','scripts/verify-voice246-last-voice-query.py')
 inputs=ROOT/'Tests/ReferenceVectors/voice246-last-voice-query-input.bin'
 oracle=ROOT/'Tests/ReferenceVectors/voice246-last-voice-query-output.bin'
 data=inputs.read_bytes();prior=oracle.read_bytes()
 if len(data)!=q.CASES*q.RECORD or len(prior)!=q.CASES*q.RESULT:raise RuntimeError('Invalid baseline dimensions')
 coverage=q.observations(data,prior) # Pre-execution eligibility for non-error queries.
 mapper=module('ldr_map','scripts/map-op1-ldr.py');raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw)
 base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,addr,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=addr and addr+n<=base+len(image) and not flags&0x1000:
   image[addr-base:addr-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(data)
 seed=0x424c4f43;words=[]
 for _ in range(BUFFER_BYTES//4):seed=(seed*1664525+1013904223)&0xffffffff;words.append(seed)
 buffer=struct.pack('<%dI'%len(words),*words)
 (out/'buffers.bin').write_bytes(buffer);(out/'raw-input.bin').write_bytes(struct.pack('<%dI'%len(RAW),*RAW))
 new_result=q.RESULT+4
 source=(ROOT/'research/blackfin/voice-last-voice-query.s.in').read_text()
 anchor='P4.H = inputs;\nP4.L = inputs;'
 prefix=f'''P1.H = buffer_input;
P1.L = buffer_input;
imm32 P2, {BUFFER_START}
imm32 P3, {BUFFER_BYTES//4}
LSETUP(buffer_in,buffer_in_end) LC0 = P3;
buffer_in: R0 = [P1++];
buffer_in_end: [P2++] = R0;
R0.H = buffer_args;
R0.L = buffer_args;
P0 = 5;
EXCPT 0;
P4.H = raw_input;
P4.L = raw_input;
P5.H = raw_output;
P5.L = raw_output;
R6 = {len(RAW)};
raw_loop:
R0 = [P4++];
[P5++] = R0;
imm32 P0, 0x01a4eec0
CALL (P0);
[P5++] = R0;
R6 += -1;
CC = R6 == 0;
IF !CC JUMP raw_loop;
R0.H = raw_args;
R0.L = raw_args;
P0 = 5;
EXCPT 0;
'''
 source=replace_once(source,anchor,prefix+anchor)
 source=replace_once(source,'[P5] = R0;\nP2 = P5;\nP2 += 4;',
   '[P5] = R0;\nimm32 P0, 0x01a4eec0\nCALL (P0);\n[P5+4] = R0;\nP2 = P5;\nP2 += 8;')
 source=replace_once(source,'EXCPT 0;\nexit 0','EXCPT 0;\nR0.H = buffer_args;\nR0.L = buffer_args;\nP0 = 5;\nEXCPT 0;\nexit 0')
 source=replace_once(source,'inputs: .incbin',f'buffer_args: .long 1,{BUFFER_START},{BUFFER_BYTES}\nraw_args: .long 1,raw_output,{len(RAW)*8}\nbuffer_input: .incbin "@WORK@/buffers.bin"\nraw_input: .incbin "@WORK@/raw-input.bin"\nraw_output: .space {len(RAW)*8}\ninputs: .incbin')
 old='.incbin "@WORK@/sdram.bin", 0, 0x00a863a8'
 spans=[(0xa4eec0,0xa4eee4,'voice_address_accessor'),(0xa4f8a8,0xa4fef8,'candidate_graphics_container')]
 parts=[];pos=0
 for start,end,symbol in spans:
  parts += [f'.incbin "@WORK@/sdram.bin", {pos}, {start-pos}',f'.global {symbol}',f'.type {symbol}, STT_FUNC',symbol+':',f'.incbin "@WORK@/sdram.bin", {start}, {end-start}',f'.size {symbol}, .-{symbol}'];pos=end
 parts += [f'.incbin "@WORK@/sdram.bin", {pos}, {0xa863a8-pos}']
 source=replace_once(source,old,'\n'.join(parts));load,save=q.caller()
 for key,value in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'RECORD':str(q.RECORD),'BYTES':str(q.CASES*new_result)}.items():source=source.replace('@'+key+'@',value)
 (out/'probe.s').write_text(source)
 def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
 templates=ROOT/'research/blackfin'
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s'])
 run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 watches=['0x01a4f8a8,0x01a4fef7','0x01919aa8','0xffa00000,0xffafffff']
 guards=[arg for pc in watches for arg in ['--watch-pc-int',pc]]
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],stdout=stdout,stderr=stderr)
 output=(out/'output.bin').read_bytes()
 if len(output)!=BUFFER_BYTES*2+len(RAW)*8+q.CASES*new_result:raise RuntimeError('Incomplete private output')
 if output[:BUFFER_BYTES]!=buffer or output[-BUFFER_BYTES:]!=buffer:raise RuntimeError('Six slabs/guards changed')
 at=BUFFER_BYTES
 for word in RAW:
  actual_input,pointer=struct.unpack_from('<2I',output,at);at+=8
  if actual_input!=word or pointer!=address(word):raise RuntimeError('Accessor raw-word arithmetic differs')
 for c in range(q.CASES):
  e=output[at+c*new_result:at+(c+1)*new_result];old=prior[c*q.RESULT:(c+1)*q.RESULT];voice=struct.unpack_from('<I',old)[0]
  if e[:4]!=old[:4] or e[8:]!=old[4:] or struct.unpack_from('<I',e,4)[0]!=address(voice):raise RuntimeError('Query -> accessor return/shared image differs')
 report={'ldr_sha256':expected,'accessor':{'entry':'0x01a4eec0','end_exclusive':'0x01a4eee4','bytes':36,'routine_sha256':hashlib.sha256(image[0xa4eec0:0xa4eee4]).hexdigest(),'formula':'(0x010225ec + raw_u32 * 0xc000) modulo 2^32','dereferences_returned_pointer':False,'index_guard_present':False,'raw_calls':len(RAW),'query_linked_calls':q.CASES,'valid_query_pointers':[hex(address(v)) for v in range(6)]},
  'container':{'entry':'0x01a4f8a8','end_exclusive':'0x01a4fef8','bytes':0x650,'routine_sha256':hashlib.sha256(image[0xa4f8a8:0xa4fef8]).hexdigest(),'query_callsite':'0x01a4feaa','accessor_callsite':'0x01a4feae','executed':False,'entry_pointer_word':'0x01911910','next_entry_pointer_word':'0x01000380','activity_gate':'0x01a27e48 reads six bytes at 0xff80105c','use':'16 signed16 reads with offset step 2; updates 16 object words starting at +0x80, then common geometry/drawing path','graphics_classification':'Strong static evidence; no original class name, complete callback contract or display feature reconstructed.'},
  'private_monitors':{'six_slabs_and_guards_start':hex(BUFFER_START),'six_slabs_and_guards_bytes':BUFFER_BYTES,'before_after_unchanged':True,'shared_bytes_per_query':q.IMAGE,'existing_query_return_and_full_state_match':True,'caller_order':'Own query -> accessor calls, not execution from the middle of the original container.','shared_regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in q.REGIONS]},
  'query_coverage':coverage,
  'watches':watches,'model':'GNU BF524 with both documented writeback patches; not physical timing',
  'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},
  'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'private_artifacts':{name:{'bytes':(out/name).stat().st_size,'sha256':sha(out/name)} for name in ['inputs.bin','buffers.bin','raw-input.bin','output.bin']},
  'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in [Path(__file__).resolve(),ROOT/'scripts/verify-voice246-last-voice-query.py',templates/'voice-last-voice-query.s.in',inputs,oracle]},
  'limits':['No container/graphics/UI execution or implementation. The activity gate and draw dependencies are static observations only.',
            'The accessor only calculates an address; slab producers, meaning, validity/lifetime, pitch/ADSR handoff and DSP integration are not recovered.',
            'Raw out-of-range words are arithmetic stress only; resulting pointers are never dereferenced. Original query invalid fallback/error path is excluded.',
            'No new Source implementation, CMake suite, public numeric fixtures, plugin change or reserved coordinator path. Firmware/instructions/disassembly/ELF remain private.']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS: accessor',len(RAW),'raw calls;',q.CASES,'query links;',BUFFER_BYTES,'slab/guard bytes unchanged. Container not executed.')
if __name__=='__main__':main()
