#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Complete selector0 caller with nonzero explicit pitch, free and busy paths.
Explicit activity/ages and tuning; no outer dispatcher, lifecycle, render,
pending-field consumer, encrypted instructions or scheduler.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path)
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
held=module('merged_held','scripts/verify-drwave246-held-note-modes.py')
explicit=module('merged_explicit','scripts/verify-drwave246-empty-explicit-pitch.py')
REGIONS,OFF,IMAGE=held.REGIONS,held.OFF,held.IMAGE
u32,put,sha=held.u32,held.put,held.sha
PITCHES,TUNINGS=explicit.PITCHES,explicit.TUNINGS
PROBES=len(PITCHES)*len(TUNINGS)
MATRIX=PROBES*8
EARLY,STREAMS,CALLS=192,6,64
INDEPENDENT=MATRIX+EARLY
CASES=INDEPENDENT+STREAMS*CALLS
TABLE_OFFSET=40+IMAGE
RECORD,RESULT=TABLE_OFFSET+512,IMAGE+32+512
WATCHES=['0x01a847e6','0x01a85750','0x01a84450,0x01a8445d','0x01a84504,0x01a84511','0x01919aa8','0xffa00000,0xffafffff']
def signed(v):return v if v<0x80000000 else v-0x100000000
def winner(ages):
 result=-1;greatest=-1
 for i,age in enumerate(ages):
  if signed(age)>greatest:greatest=signed(age);result=i
 return result
def age_pattern(pattern):
 desired=pattern%6;kind=(pattern//6)%5
 if kind==0:ages=[0xffffffff]*6;ages[desired]=0
 elif kind==1:ages=[7]*6
 elif kind==2:ages=[0xffffffff,0x80000000,0,1,2,3];ages[desired]=0x7fffffff
 elif kind==3:ages=[0xffffffff]*6;ages[desired]=100;ages[(desired+1)%6]=100
 else:ages=[(i-desired-1)%6+1 for i in range(6)]
 return ages

def inputs():
 context=held.inputs();data=bytearray();seed=0
 def rnd():
  nonlocal seed
  seed=(seed*1664525+1013904223)&0xffffffff;return seed
 for c in range(CASES):
  if c<MATRIX:
   pattern,member=divmod(c,8);profile=member//2;variant=member%2
   busy=profile>=2;matched=profile%2==0;kind=int(busy);reset=1;gate=0
   pitch=PITCHES[pattern//len(TUNINGS)];tuning=TUNINGS[pattern%len(TUNINGS)];note=pattern%128
  elif c<INDEPENDENT:
   pattern=c-MATRIX;busy=True;matched=pattern%2==0;variant=pattern%2
   kind=2 if pattern<96 else 3;reset=1;gate=[1,128,255][(pattern//3)%3] if kind==2 else 0
   pitch=PITCHES[pattern%len(PITCHES)];tuning=TUNINGS[pattern%len(TUNINGS)];note=pattern%128
  else:
   stream,step=divmod(c-INDEPENDENT,CALLS);pattern=1000+stream*64+step
   busy=bool(stream%2) if stream<2 else bool((stream+step)%2);matched=step%4<2
   kind=3 if step%8==1 else 2 if step%8==2 else int(busy)
   variant=step%2;reset=int(step==0);gate=255 if kind==2 else 0
   pitch=PITCHES[(step*7+stream)%len(PITCHES)];tuning=TUNINGS[(step+stream)%len(TUNINGS)]
   note=step-1 if kind==3 else step
  seed=(0x504f4c58^(pattern*0x9e3779b9))&0xffffffff
  image=bytearray(context[(pattern%held.CASES)*held.RECORD+40:(pattern%held.CASES)*held.RECORD+40+IMAGE])
  header=bytearray(struct.pack('<10I',*[rnd() for _ in range(10)]))
  put(header,0,reset);put(header,4,kind);put(header,16,note)
  velocity=[0,1,127,128,255,32767,32768,65535][(pattern//3)%8]
  put(header,20,(rnd()&0xffff0000)|velocity);header[24]=[0,1,128,255][(pattern//5)%4];put(header,28,pitch)
  selected=pattern%8;put(image,4392,selected);put(image,4400,selected if matched else (selected+1)%8)
  struct.pack_into('<h',image,4240+selected*16+8,[0,1,2048,4095][pattern%4])
  put(image,4,(rnd()&0xffffff00)|gate);put(image,3468,0);put(image,2576,1)
  ages=age_pattern(pattern) if busy else [0xffffffff,0x80000000,0xffffffff,0x80000000,0xffffffff,0x80000000]
  if kind in (2,3):ages=[0xffffffff,0x80000000]*3
  mask=63 if busy or kind in (2,3) else pattern%63
  target=winner(ages) if mask==63 else next(i for i in range(6) if not mask&(1<<i))
  if target<0:target=pattern%6 # Only early-return fixtures; selection excluded.
  for v in range(6):
   image[3504+v]=[1,128,255][(pattern+v)%3] if mask&(1<<v) else 0
   put(image,2580+v*24,[0,1,3,6,7,8,0xffffffff][(pattern+v)%7]);put(image,2584+v*24,ages[v])
   put(image,4528+v*4,0)
  put(image,4560,0);put(image,4444,tuning);put(image,4456,1200)
  if reset:
   n=pattern%128 if kind<2 else pattern%128+1
   if c>=INDEPENDENT:n=17
   put(image,2568,n)
   policy=(pattern//6)%4
   for i in range(128):
    put(image,8+i*20,0x10000000+i)
    owner=6+i
    if policy==1 and i==n//2:owner=target
    elif policy==2 and i in (0,n-1):owner=target
    elif policy==3:
     if i==0:owner=0x10000|target
     elif i==n-1:owner=target
    put(image,8+i*20+16,owner);image[8+i*20+8]=[0,1,128,255][(pattern+i)%4]
   if n:put(image,8,0x10000000|note)
   if kind==3:
    put(image,8+[0,n//2,n-1][(pattern//3)%3]*20,note)
    if n>1 and pattern%2==0:put(image,8+(n-1)*20,note)
  table=struct.pack('<128I',*[(rnd()|1) if variant else 0 for _ in range(128)])
  data+=header+image+table
 return data

def loaded_image(previous,row):
 image=held.loaded_image(previous,row)
 for v in range(6):put(image,2584+v*24,u32(row,40+2584+v*24))
 image[:4]=row[TABLE_OFFSET+508:TABLE_OFFSET+512]
 return image

def observations(data,output=None):
 if len(data)!=CASES*RECORD:raise RuntimeError('Wrong input dimensions')
 if output is not None and len(output)!=CASES*RESULT:raise RuntimeError('Incomplete original output')
 for p in range(PROBES):
  for profile in range(4):
   c=p*8+profile*2;a=data[c*RECORD:(c+1)*RECORD];b=data[(c+1)*RECORD:(c+2)*RECORD]
   if a[:TABLE_OFFSET]!=b[:TABLE_OFFSET] or a[TABLE_OFFSET:]==b[TABLE_OFFSET:]:raise RuntimeError('Lookup matrix is not controlled')
   if output is not None:
    x=output[c*RESULT:(c+1)*RESULT];y=output[(c+1)*RESULT:(c+2)*RESULT]
    if x[4:IMAGE+32]!=y[4:IMAGE+32]:raise RuntimeError('Explicit result depends on lookup table')
  for first,second in [(0,2),(4,6)]:
   a=bytearray(data[(p*8+first)*RECORD:(p*8+first+1)*RECORD]);b=bytearray(data[(p*8+second)*RECORD:(p*8+second+1)*RECORD])
   a[40+4400:40+4404]=b[40+4400:40+4404]=b'\0'*4
   if a!=b:raise RuntimeError('Core accept/reject inputs differ beyond loaded slot')
  if output is not None:
   a=bytearray(output[(p*8+4)*RESULT:(p*8+5)*RESULT]);b=bytearray(output[(p*8+6)*RESULT:(p*8+7)*RESULT])
   a[4400:4404]=b[4400:4404]=b'\0'*4
   if a!=b:raise RuntimeError('Busy helper depends on Core acceptance')
 image=bytearray(IMAGE)
 counts=dict(free=0,busy=0,gated=0,duplicate=0,accepted=0,blocked=0,detached=0,duplicate_owners=0,tied=0,early_all_negative=0)
 free=set();victims=set();lengths=set();pitches=set()
 for c in range(CASES):
  row=data[c*RECORD:(c+1)*RECORD];image=loaded_image(image,row)
  n=u32(image,2568);note=u32(row,16);selected=u32(image,4392);loaded=u32(image,4400)
  if n>128 or note>=128 or not u32(row,28) or selected>=8 or loaded>=8:raise RuntimeError('Invalid bounded raw arguments/list')
  mode=struct.unpack_from('<h',image,4240+selected*16+8)[0]//4096
  if mode or u32(image,3468) or u32(image,2576)!=1 or any(u32(image,4528+v*4) for v in range(6)) or u32(image,4560):raise RuntimeError('Invalid bounded mode/dependencies')
  active=image[3504:3510];ages=[u32(image,2584+v*24) for v in range(6)];owners=[u32(image,8+i*20+16) for i in range(n)]
  duplicate=any(u32(image,8+i*20)==note for i in range(n))
  kind=2 if image[4] else 3 if duplicate else 1 if all(active) else 0
  if kind!=u32(row,4):raise RuntimeError('Generator branch classification differs')
  target=-1;lengths.add(n);pitches.add(u32(row,28))
  if kind>=2:
   counts['gated' if kind==2 else 'duplicate']+=1
   counts['early_all_negative']+=all(signed(a)<0 for a in ages)
  else:
   if n==128:raise RuntimeError('Would reach excluded capacity assertion')
   if kind==0:
    target=next(v for v in range(6) if not active[v]);free.add(target);counts['free']+=1
    counts['accepted' if loaded==selected else 'blocked']+=1
   else:
    target=winner(ages)
    if target<0:raise RuntimeError('Would reach excluded all-negative-age assertion')
    victims.add(target);counts['busy']+=1
    greatest=max(map(signed,ages));counts['tied']+=sum(signed(a)==greatest for a in ages)>1
    counts['detached']+=target in owners;counts['duplicate_owners']+=owners.count(target)>1
  if output is not None:
   e=output[c*RESULT:(c+1)*RESULT]
   if e[IMAGE:IMAGE+32]!=row[8:40] or e[IMAGE+32:]!=row[TABLE_OFFSET:]:raise RuntimeError('Argument/guards/table changed')
   if u32(e,2568)!=n+(kind<2) or e[4368:4376]!=image[4368:4376]:raise RuntimeError('Count transition or RNG preservation differs')
   image[:]=e[:IMAGE]
  elif kind<2:
   if kind==1:
    for i,owner in enumerate(owners):
     if owner==target:put(image,8+i*20+16,0xffffffff);break
   p=8+n*20;image[p:p+9]=row[16:25];image[p+12:p+16]=row[28:32];put(image,p+16,target);put(image,2568,n+1)
 if free!=set(range(6)) or victims!=set(range(6)) or pitches!=set(PITCHES):raise RuntimeError('Missing free/victim/pitch coverage')
 return {'counts':counts,'free_voices':sorted(free),'victims':sorted(victims),'entry_lengths':sorted(lengths),'pitch_patterns':len(pitches),'lookup_invariant_pairs':PROBES*4,'busy_core_invariant_pairs':PROBES,'rng_unchanged_all_calls':True}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
 if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output outside checkout')
 expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
 if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 LDR supported')
 data=inputs();coverage=observations(data)
 mapper=module('ldr_mapper','scripts/map-op1-ldr.py');raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw)
 base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,addr,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=addr and addr+n<=base+len(image) and not flags&0x1000:image[addr-base:addr-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(data)
 templates=ROOT/'research/blackfin';load,save=held.caller();source=(templates/'drwave-poly-explicit-pitch.s.in').read_text()
 ages=''.join(f'P0 = {40+2584+v*24};\nP1 = P4 + P0;\nR0 = [P1];\nimm32 P1, {0xff8000d8+v*24}\n[P1] = R0;\n' for v in range(6))
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'AGES':ages,'TABLE_OFFSET':str(TABLE_OFFSET),'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Both documented GNU patches required')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 guards=[x for pc in WATCHES for x in ['--watch-pc-int',pc]]
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],stdout=stdout,stderr=stderr)
 output=(out/'output.bin').read_bytes();actual=observations(data,output)
 if actual!=coverage:raise RuntimeError('Observed coverage differs from preflight')
 artifacts={}
 for name,path in [('drwave246-poly-explicit-pitch-input.bin',out/'inputs.bin'),('drwave246-poly-explicit-pitch-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 sources=[Path(__file__).resolve(),templates/'drwave-poly-explicit-pitch.s.in',ROOT/'Source/DrWave246PolyExplicitPitch.h',ROOT/'Tests/DrWave246PolyExplicitPitchTests.cpp',ROOT/'scripts/verify-drwave246-held-note-modes.py',ROOT/'scripts/verify-drwave246-empty-explicit-pitch.py',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py']
 sources += [ROOT/'Source'/s for s in ['DrWave246Activation.h','DrWave246Routing.h','DrWave246Kernel.h','DrWave246.h','Voice246Activation.h','Voice246NoteArgument.h','Voice246PolyRelease.h','Envelope246Transition.h','Envelope246.h','Envelope246Tables.h','ReferenceDsp.h']]
 routines={name:{'entry':hex(start),'end_exclusive':hex(end),'bytes':end-start,'routine_sha256':hashlib.sha256(image[start-base:end-base]).hexdigest()} for name,start,end in [('free_helper',0x1a84414,0x1a844d8),('busy_helper',0x1a844d8,0x1a84604),('caller',0x1a84604,0x1a84ba0)]}
 report={'ldr_sha256':expected,'routines':routines,'records':CASES,'independent':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,'matrix_records':MATRIX,'pitch_words':[hex(x) for x in PITCHES],'tuning_words':[hex(x) for x in TUNINGS],'coverage':actual,'image_bytes':IMAGE,'input_record_bytes':RECORD,'output_record_bytes':RESULT,'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in sources},'pc_watches':WATCHES,'model':'GNU BF524 with both documented writeback patches; no hardware/timing claim','limits':['Only complete caller selector0 with nonzero explicit pitch, raw note0..127, six ADSR ready, outer mode0 and absent auxiliary modules.','New entry at count128, all-negative busy ages when selection is reached, lazy init and other modes are excluded before simulation. Own adapter guards are not original assertion returns.','Arguments, activity, ages, tuning, lookup table and order are explicit; lists and other selected state carry in streams. No original allocator/scheduler or cadence inferred.','No constructor, outer dispatcher, release, ADSR/audio render, pending-field consumer or plugin integration. Register/pending writes do not establish render-buffer control.','Only selected shared RAM regions plus table/arguments/guards monitored; no complete CPU/stack/hardware claim. Firmware/instructions/disassembly/ELF private.']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS original poly explicit:',CASES,'records;',actual['counts'])
if __name__=='__main__':main()
