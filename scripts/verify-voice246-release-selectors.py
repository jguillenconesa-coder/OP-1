#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original VoiceCtl selector release followed by explicitly scheduled ADSR render.
No encrypted L1 execution, original allocator/clock or pitch-buffer producer.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=772
# Address, bytes, retain across the four 64-call streams.
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,False),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),(0x018c2180,12,False)]
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=32+IMAGE+272;RESULT=IMAGE*2+308
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
 data=bytearray();seed=0x53454c52
 def rnd():
  nonlocal seed
  seed=(seed*1664525+1013904223)&0xffffffff;return seed
 def put(b,o,v):struct.pack_into('<I',b,o,v&0xffffffff)
 for c in range(CASES):
  b=bytearray(struct.pack('<%dI'%(IMAGE//4),*[rnd() for _ in range(IMAGE//4)]))
  count=c%129;selected=c%8;loaded=(c//8)%8;voice=c%6;mode=(c//129)%4+1
  if c>=516:mode=(c-516)//64+1
  put(b,0,610832681);put(b,4,0);put(b,2568,count) # Leading neighbor is note-table word 127.
  for i in range(128):
   p=8+i*20;put(b,p,i%128);put(b,p+16,((i+c)%8)-2);put(b,p+12,0 if (c//4)%2 else [1,0x7fffffff,0x80000000,rnd()][i%4])
  # First/middle/last, duplicate and absent lookups, including the full list.
  index=(0,count//2,max(0,count-1),0)[c%4]
  note=0x80000001 if c%4==3 or not count else index%128
  if count and c%4==2:
   note=(count-1)%128
   if count>2 and c%2==0:put(b,8,note) # Earlier duplicate; last match still wins.
  if c>=516 and (c-516)%64==0:
   count=1 if (c-516)//64%2 else 64;put(b,2568,count);note=count-1
  for i in range(count):
   if (c//4)%2==0 and i%7==0:put(b,8+i*20,0xffffffff) # Explicit pitch bypasses lookup.
  if c%4==0 and count:put(b,8+index*20,note)
  if count==1:note=struct.unpack_from('<I',b,8)[0]
  if count>2 and c%4==2:put(b,8+20,note)
  for i in range(6):
   p=OFF[1]+4+i*24
   for j,v in enumerate([c%9,rnd(),[0,16,32768,536870912,1073741824][c%5],rnd(),rnd(),(rnd()&0xffff0000)|(rnd()%32768)]):put(b,p+j*4,v)
   put(b,OFF[2]+i*36,0x0190fb2c)
  put(b,OFF[3]+4,0) # VoiceCtl mode ff801038
  for i in range(8):struct.pack_into('<4h',b,OFF[4]+i*16,rnd()%16384,rnd()%16384,[0,1,8192,16384,32767][(c+i)%5],rnd()%16384)
  for i in range(8):struct.pack_into('<h',b,OFF[4]+i*16+8,mode*4096+(c%4096)) # Explicit selector + low bits
  put(b,OFF[6],rnd()%65536-32768);put(b,OFF[7],1);put(b,OFF[8],selected);put(b,OFF[9],loaded)
  for i in range(6):put(b,OFF[10]+4+i*4,0xffb00000+i*36)
  put(b,OFF[11]+4,[0,1,0x40000000,0x7fffffff,0x80000000,0xffffffff,rnd()][c%7]);put(b,OFF[12]+4,1200)
  header=struct.pack('<8I',int(c<516 or (c-516)%64==0),voice,rnd()%32768,0,note,rnd(),rnd(),rnd())
  data+=header+b+struct.pack('<68I',*[rnd() for _ in range(68)])
 return data

def caller():
 def copy(label,src,dst,n):
  return f'{src}\n{dst}\nP3 = {n//4};\nLSETUP({label},{label}_end) LC0 = P3;\n{label}: R0 = [P1++];\n{label}_end: [P2++] = R0;\n'
 load='';save=''
 for i,(addr,n,retain) in enumerate(REGIONS):
  if retain:load+=f'R0 = [P4];\nCC = R0 == 0;\nIF CC JUMP skip_{i};\n'
  load+=copy('load_'+str(i),f'P0 = {32+OFF[i]};\nP1 = P4 + P0;',f'imm32 P2, {addr}',n)
  if retain:load+=f'skip_{i}:\n'
  save+=copy('save_'+str(i),f'imm32 P1, {addr}','P2 = P5;',n)+'P5 = P2;\n'
 return load,save

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
 if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
 expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
 if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 supported')
 spec=importlib.util.spec_from_file_location('mapper',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 raw=a.ldr.read_bytes();meta,_=m.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,addr,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=addr and addr+n<=base+len(image) and not flags&0x1000:image[addr-base:addr-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(mode=0o700,parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
 load,save=caller();templates=ROOT/'research/blackfin'
 conversion=''.join(f'imm32 P0, {0x018c13c4+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0x0106b3cc+i*4}\n[P0] = R0;\n' for i in range(256))
 source=(templates/'voice-release-selectors.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'BEFORE':save.replace('save_','before_'),'AFTER':save.replace('save_','after_'),'COEFFICIENTS':conversion,'RECORD':str(RECORD),'IMAGE':str(IMAGE),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
 if (out/'output.bin').stat().st_size!=CASES*RESULT+1024:raise RuntimeError('Incomplete output')
 counts={'list_changes':0,'pitch_changes':0,'rng_changes_before_render':0,'adsr_state_changes':0,'written_samples':0,'preserved_samples':0,'retune_register_writes':0,'core_adsr_release_calls':0,'older_entry_removals':0,'last_duplicate_removals':0,'lookup_retunes':0,'explicit_retunes':0,'explicit_out_of_range_retunes':0}
 all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Original note table copy/preservation differs')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes();prior=None
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  before=x[32:32+IMAGE] if struct.unpack_from('<I',x)[0] else prior
  n=struct.unpack_from('<I',before,2568)[0];note=struct.unpack_from('<I',x,16)[0]
  selected=struct.unpack_from('<I',x,32+4392)[0];mode=struct.unpack_from('<h',x,32+4240+selected*16+8)[0]>>12
  match=next((i for i in range(n) if struct.unpack_from('<I',before,8+i*20)[0]==note),None)
  if mode!=4 and match is not None:
   if struct.unpack_from('<I',before,8+(n-1)*20)[0]==note:
    voices=6 if mode==3 else 1
    counts['last_duplicate_removals']+=match!=n-1
    if n==1:counts['core_adsr_release_calls']+=voices
    else:
     counts['retune_register_writes']+=voices
     pitch=struct.unpack_from('<I',before,8+(n-2)*20+12)[0]
     counts['explicit_retunes' if pitch else 'lookup_retunes']+=1
     counts['explicit_out_of_range_retunes']+=pitch!=0 and struct.unpack_from('<I',before,8+(n-2)*20)[0]>=128
   else:counts['older_entry_removals']+=1
  counts['list_changes']+=before[:2576]!=e[:2576]
  # Registers are explicit every call; retained regions above come from prior render.
  counts['pitch_changes']+=x[32+4136:32+4160]!=e[4136:4160]
  counts['rng_changes_before_render']+=before[4368:4372]!=e[4368:4372]
  counts['adsr_state_changes']+=before[2580:2724]!=e[2580:2724]
  v=struct.unpack_from('<I',x,4)[0];stage=struct.unpack_from('<I',e,2580+v*24)[0]
  counts['written_samples' if 1<=stage<=8 else 'preserved_samples']+=128
  prior=e[IMAGE:IMAGE*2]
 artifacts={}
 for name,path in [('voice246-release-selectors-input.bin',out/'inputs.bin'),('voice246-release-selectors-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'routine':'0x01a85058','routine_end_exclusive':'0x01a8541c','selectors':[1,2,3,4],'records':CASES,'independent':516,'streams':4,'blocks_per_stream':64,'sample_positions':CASES*128,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Flag and outer VoiceCtl mode zero; per-sound selectors 1..4. Initialized Core/ADSR pools and explicit tuning index 1200 coefficient.','Following ADSR render, list seeds, controls, registers, selected/loaded sounds and event schedule are harness inputs. No pitch-buffer producer, original scheduler, mixer, output or complete integration.','First-four envelope controls valid, nonnegative level/gain/volume/sustain. Error paths excluded. No commercial identity assigned to selector values.','Dr. Wave release is executed and complete objects preserved; its audio render is not called here. Firmware, instructions, disassembly and ELF stay private.']}
 report['note_table']={'source':'0x018c21b0','destination':'0x01dfe530','words':128,'copy_fragment_start':'0x01a86700','copy_fragment_end_exclusive':'0x01a8672e','copy_fragment_sha256':hashlib.sha256(image[0x01a86700-base:0x01a8672e-base]).hexdigest(),'scope':'Original copy fragment plus own RTS, not complete initializer; full table captured before/after. Leading list neighbor 0x01dfe72c is note word 127.'}
 report['tuning']={'index_address':'0x018c2184','explicit_index':1200,'coefficient_address':'0x027fd9e0','coefficient':'Explicit per-call input including zero, one, signs and extremes; no default calibration inferred.'}
 report['model']='GNU BF524 with both documented D-register/disjoint-halfword writeback patches; not hardware or pipeline validation'
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [Path(__file__).resolve(),templates/'voice-release-selectors.s.in',ROOT/'Source/Envelope246.h',ROOT/'Source/Voice246PolyRelease.h',ROOT/'Source/Voice246ReleaseSelectors.h',ROOT/'Tests/Voice246ReleaseSelectorsTests.cpp']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original selector release:',CASES,'records; image',IMAGE,'record',RECORD,'result',RESULT)
if __name__=='__main__':main()
