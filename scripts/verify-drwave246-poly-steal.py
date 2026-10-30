#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original busy poly caller: signed age selection, ADSR restart and list reassignment.
No encrypted L1 execution, original allocator/clock or pitch-buffer producer.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=768
# Address, bytes, retain across the four 64-call streams.
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,True),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),(0x018c2180,12,False),(0x01de4c3c,12,False),(0x01dfe4d8,48,True),(0x01aaad68,32,False),(0x01aaac40,12,False)]
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=40+IMAGE+272;RESULT=IMAGE*2+364
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
 spec=importlib.util.spec_from_file_location('free',ROOT/'scripts/verify-drwave246-poly-first-free.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 old=m.inputs();data=bytearray()
 for c in range(CASES):
  row=old[c*RECORD:(c+1)*RECORD];header=bytearray(row[:40]);image=bytearray(row[40:40+IMAGE])
  step=(c-512)%4;group=(c-512)//4
  kind=0 if c<256 else 1 if c<384 else 2 if c<512 else 0 if step==0 else 1 if step==3 else 2
  target=(c-step if c>=512 and step==3 else c)%128
  desired=c%6 if c<512 else group%6;pattern=(c//6)%5 if c<512 else (group//6)%5
  if pattern==0:ages=[0xffffffff]*6;ages[desired]=0
  elif pattern==1:ages=[7]*6
  elif pattern==2:ages=[(i-desired-1)%6+1 for i in range(6)]
  elif pattern==3:ages=[0xffffffff,0x80000000,0,1,2,3];ages[desired]=0x7fffffff
  else:ages=[0xffffffff]*6;ages[desired]=100;ages[(desired+1)%6]=100
  if 256<=c<512:ages=[0xffffffff]*6
  signed=lambda x:x-0x100000000 if x&0x80000000 else x
  maximum=max(map(signed,ages));winner=next((i for i,v in enumerate(ages) if signed(v)==maximum),0) if maximum>=0 else c%6
  struct.pack_into('<I',header,4,winner);struct.pack_into('<I',header,12,kind)
  velocity=struct.unpack_from('<I',header,20)[0];struct.pack_into('<I',header,20,(velocity&0xffff0000)|[0,1,127,128,255,32767,32768,65535][c%8])
  for i,v in enumerate(ages):struct.pack_into('<I',image,2584+i*24,v)
  for i in range(6):image[3504+i]=[1,128,255][(c+i)%3]
  if 256<=c<512:image[3504+c%6]=0 # Early return precedes busy-voice/age checks.
  image[4]=int(kind==2)
  selected=struct.unpack_from('<I',image,4392)[0];disc=struct.unpack_from('<I',image,4468)[0]
  table=list(struct.unpack_from('<8i',image,4484));slot,value=struct.unpack_from('<2i',header,32);table[slot]=value
  struct.pack_into('<I',header,16,(target-53-(0 if disc==1 else table[selected]*12))&0xffffffff)
  if c<512 or (c-512)%64==0:
   n=(c%128 if kind==0 else c%128+1) if c<512 else 12
   struct.pack_into('<I',image,2568,n)
   for i in range(128):
    struct.pack_into('<I',image,8+i*20,0x10000000+i)
    owner=(winner+1+i%5)%6
    policy=(c//6)%4
    if policy==0:owner=6+i
    elif policy==1 and i==n//2:owner=winner
    elif policy==2 and i in (0,n-1):owner=winner
    elif policy==3:
     if i==0:owner=0x10000|winner
     elif i==n-1:owner=winner
    struct.pack_into('<I',image,8+i*20+16,owner)
    image[8+i*20+8]=[0,1,128,255][(c+i)%4]
   if n and kind==0:struct.pack_into('<I',image,8,0x10000000|target)
   if kind==1:struct.pack_into('<I',image,8+(c%n)*20,target)
  data+=header+image+row[-272:]
 return data

def caller():
 def copy(label,src,dst,n):
  return f'{src}\n{dst}\nP3 = {n//4};\nLSETUP({label},{label}_end) LC0 = P3;\n{label}: R0 = [P1++];\n{label}_end: [P2++] = R0;\n'
 load='';save=''
 for i,(addr,n,retain) in enumerate(REGIONS):
  if retain:load+=f'R0 = [P4];\nCC = R0 == 0;\nIF CC JUMP skip_{i};\n'
  load+=copy('load_'+str(i),f'P0 = {40+OFF[i]};\nP1 = P4 + P0;',f'imm32 P2, {addr}',n)
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
 source=(templates/'drwave-poly-steal.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'BEFORE':save.replace('save_','before_'),'AFTER':save.replace('save_','after_'),'COEFFICIENTS':conversion,'RECORD':str(RECORD),'IMAGE':str(IMAGE),'BYTES':str(CASES*RESULT),'AGES':''.join(f'P0 = {40+2584+i*24};\nP1 = P4 + P0;\nR0 = [P1];\nimm32 P1, {0xff8000d8+i*24}\n[P1] = R0;\n' for i in range(6)),'ACTIVITY':'P0 = 3544;\nP1 = P4 + P0;\nimm32 P2, 0xff80105c\nP3 = 6;\nLSETUP(activity_in,activity_in_end) LC0 = P3;\nactivity_in: R0 = B[P1++] (Z);\nactivity_in_end: B[P2++] = R0;'}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
 if (out/'output.bin').stat().st_size!=CASES*RESULT+1024:raise RuntimeError('Incomplete output')
 counts={'appended':0,'gated':0,'duplicates':0,'detached_owner':0,'duplicate_owners':0,'ties':0,'written_samples':0,'preserved_samples':0};used=set();prior=None
 all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Note table changed')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes()
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  previous=x[40:40+IMAGE] if struct.unpack_from('<I',x)[0] else prior
  n=struct.unpack_from('<I',previous,2568)[0];after=struct.unpack_from('<I',e,2568)[0];note=struct.unpack_from('<I',e,IMAGE*2+340)[0]
  if after==n+1:
   counts['appended']+=1;used.add(struct.unpack_from('<I',e,8+n*20+16)[0])
   voice=struct.unpack_from('<I',e,8+n*20+16)[0]
   owners=[struct.unpack_from('<I',previous,8+i*20+16)[0] for i in range(n)]
   counts['detached_owner']+=voice in owners;counts['duplicate_owners']+=owners.count(voice)>1
   ages=[struct.unpack_from('<i',x,40+2584+i*24)[0] for i in range(6)];counts['ties']+=ages.count(max(ages))>1
  elif x[40+4]:counts['gated']+=1
  else:counts['duplicates']+=1
  v=struct.unpack_from('<I',x,4)[0];stage=struct.unpack_from('<I',e,2580+v*24)[0]
  counts['written_samples' if 1<=stage<=8 else 'preserved_samples']+=128
  prior=e[IMAGE:IMAGE*2]
 if used!=set(range(6)):raise RuntimeError('Missing stolen voice')
 artifacts={}
 for name,path in [('drwave246-poly-steal-input.bin',out/'inputs.bin'),('drwave246-poly-steal-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'poly_caller':'0x01a84604','next_routine':'0x01a84ba0','path':'Poly selector zero with all voices busy and a nonnegative signed maximum age, or gate/duplicate early return.','constructor':'0x01a85014','setter':'0x01a8633c','activation':'0x01a84414','adsr_render':'0x01912128','records':CASES,'independent':512,'streams':4,'blocks_per_stream':64,'sample_positions':CASES*128,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Original parent executes signed age selection and its restart/detach/append helper. First-free branch, other modes, present auxiliary modules, lazy ADSR init, invalid capacity and all-negative-age error paths excluded.','Original setters/constructor and ADSR rendering have explicit harness order. Activity, six age words and gate byte are supplied per call; no original aging, voice-free schedule or clock inferred.','List, pending state, levels/gains, other registers and RNG retained in streams except explicit activity/age/gate inputs. Pending words are produced, not consumed by a recovered handoff. No Dr. Wave audio render, pitch-buffer producer, scheduler, output or host integration.','Dr. Wave vtables are valid with other stress fields, not claimed render-ready objects. Firmware/instructions/ELF remain private.']}
 report['busy_helper']={'entry':'0x01a844d8','end_exclusive':'0x01a84604','scope':'Unmodified helper invoked by original parent; zero explicit pitch and notes 0..127 in this linked oracle.'}
 report['age_selection']={'field':'ADSR offset 4','comparison':'signed32 maximum, strict greater than initial -1; first tie wins','all_negative_unique_note_path':'excluded original assertion path; C++ rejects before mutation'}
 report['pending_fields']={'offset12':'Prepared pitch from original note table and explicit tuning Q31, saturated shift 1','offset16':'Velocity low signed16 shifted/saturated 8, then uint16 shifted 16','preserved_before_render':['level','gain','Dr. Wave objects','VoiceCtl pitch/velocity/activity registers'],'runtime_consumer':'Not recovered; no invented commit to pitch buffer.'}
 report['setter_table_snapshot']={'address':'0x01dfe4d8','bytes':48,'stage':'After original setters and before constructor; table plus neighbors.'}
 report['table_supply']={'poison_before_initializing_setters':True,'initializing_setters_per_reset':8,'changing_setters_per_record':1,'direct_override_after_setters':False,'valid_slots':[0,7],'valid_adjustments':[-4,4],'schedule':'Explicit harness order, not recovered boot/settings/dispatcher cadence.'}
 report['note_table']={'source':'0x018c21b0','destination':'0x01dfe530','words':128,'copy_fragment_start':'0x01a86700','copy_fragment_end_exclusive':'0x01a8672e','copy_fragment_sha256':hashlib.sha256(image[0x01a86700-base:0x01a8672e-base]).hexdigest(),'scope':'Original copy fragment plus own RTS, not complete initializer; full table captured before/after. Leading list neighbor 0x01dfe72c is note word 127.'}
 report['tuning']={'index_address':'0x018c2184','explicit_index':1200,'coefficient_address':'0x027fd9e0','coefficient':'Explicit per-call input including zero, one, signs and extremes; no default calibration inferred.'}
 report['model']='GNU BF524 with both documented D-register/disjoint-halfword writeback patches; not hardware or pipeline validation'
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [Path(__file__).resolve(),templates/'drwave-poly-steal.s.in',ROOT/'Source/Envelope246.h',ROOT/'Source/Voice246PolyRelease.h',ROOT/'Source/Voice246ReleaseSelectors.h',ROOT/'Source/Voice246NoteArgument.h',ROOT/'Source/DrWave246Activation.h',ROOT/'Source/Voice246Activation.h',ROOT/'Tests/DrWave246PolyStealTests.cpp',ROOT/'Source/DrWave246PolySteal.h',ROOT/'Source/Envelope246Transition.h',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-voice246-release-selectors.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'Source/Voice246NoteAdjustment.h',ROOT/'Source/Voice246NoteTable.h',ROOT/'Source/DrWave246Routing.h',ROOT/'Source/DrWave246.h',ROOT/'Source/ReferenceDsp.h',ROOT/'Source/Envelope246Tables.h',templates/'voice-control-input.ld',templates/'parallel-halfword.s']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original poly busy-voice caller:',CASES,'records; image',IMAGE,'record',RECORD,'result',RESULT)
if __name__=='__main__':main()
