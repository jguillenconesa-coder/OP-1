#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original complete caller: empty list and nonzero raw explicit pitch only.
Synthetic lookup-table pairs prove bypass; no constructor, lifecycle, render,
pending-field consumer, encrypted code, scheduler or host integration.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path)
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
held=module('merged_held','scripts/verify-drwave246-held-note-modes.py')
REGIONS,OFF,IMAGE=held.REGIONS,held.OFF,held.IMAGE
u32,put,sha=held.u32,held.put,held.sha
PITCHES=list(dict.fromkeys([1<<b for b in range(32)]+[0xffffffff^(1<<b) for b in range(32)]+[
 0xffffffff,0xfffffffe,0x3fffffff,0x40000000,0x40000001,0xbfffffff,0xc0000000,0xc0000001,
 0x7ffffffe,0x7fffffff,0x80000000,0x80000001]))
TUNINGS=[0,1,0x7fffffff,0x80000000,0xffffffff,0x40000000,0xc0000000]
PROBES=len(PITCHES)*len(TUNINGS)
MATRIX=PROBES*6
GATES,STREAMS,CALLS=96,6,64
INDEPENDENT=MATRIX+GATES
CASES=INDEPENDENT+STREAMS*CALLS
TABLE_OFFSET=40+IMAGE
RECORD,RESULT=TABLE_OFFSET+512,IMAGE+32+512
WATCHES=['0x01a84a70','0x01a84690','0x01a84814,0x01a84821','0x01919aa8','0xffa00000,0xffafffff']

def inputs():
 context=held.inputs();data=bytearray();seed=0
 def rnd():
  nonlocal seed
  seed=(seed*1664525+1013904223)&0xffffffff;return seed
 for c in range(CASES):
  if c<MATRIX:
   pattern,member=divmod(c,6);selector=member//2+1;variant=member%2
   pitch=PITCHES[pattern//len(TUNINGS)];tuning=TUNINGS[pattern%len(TUNINGS)]
   reset=1;gate=0
  elif c<INDEPENDENT:
   pattern=c-MATRIX;selector=pattern%3+1;variant=pattern%2
   pitch=PITCHES[pattern%len(PITCHES)];tuning=TUNINGS[pattern%len(TUNINGS)]
   reset=1;gate=[1,128,255][(pattern//3)%3]
  else:
   stream,step=divmod(c-INDEPENDENT,CALLS);pattern=1000+stream*64+step
   selector=stream%3+1 if stream<3 else step%3+1;variant=step%2
   pitch=PITCHES[(step*7+stream)%len(PITCHES)];tuning=TUNINGS[(step+stream)%len(TUNINGS)]
   reset=int(step==0);gate=255 if step%8==7 else 0
  seed=(0x4558504c^(pattern*0x9e3779b9))&0xffffffff
  image=bytearray(context[(pattern%held.CASES)*held.RECORD+40:(pattern%held.CASES)*held.RECORD+40+IMAGE])
  header=bytearray(struct.pack('<10I',*[rnd() for _ in range(10)]))
  put(header,0,reset);put(header,4,int(gate!=0));put(header,16,pattern%128)
  velocity=[0,1,127,128,255,32767,32768,65535][(pattern//3)%8]
  put(header,20,(rnd()&0xffff0000)|velocity);header[24]=[0,1,128,255][(pattern//5)%4]
  put(header,28,pitch)
  selected=pattern%8;put(image,4392,selected)
  put(image,4400,selected if (pattern//64)%2==0 else (selected+1)%8)
  struct.pack_into('<h',image,4240+selected*16+8,selector*4096+[0,1,2048,4095][pattern%4])
  put(image,4,(rnd()&0xffffff00)|gate);put(image,2568,0);put(image,3468,0);put(image,2576,1)
  mask=pattern%64
  for v in range(6):
   image[3504+v]=[1,128,255][(pattern+v)%3] if mask&(1<<v) else 0
   put(image,2580+v*24,[0,1,3,6,7,8,0xffffffff][(pattern+v)%7])
   put(image,2584+v*24,[0,1,0x7fffffff,0x80000000,0xffffffff][(pattern+v)%5])
   put(image,4528+v*4,0)
  put(image,4560,0);put(image,4444,tuning);put(image,4456,1200)
  table=struct.pack('<128I',*[(rnd()|1) if variant else 0 for _ in range(128)])
  data+=header+image+table
 return data

def loaded_image(previous,row):
 image=held.loaded_image(previous,row)
 put(image,2568,0) # Explicit reset, not an original list producer/lifecycle.
 image[:4]=row[TABLE_OFFSET+508:TABLE_OFFSET+512] # Real overlapping neighbor.
 return image

def observations(data,output=None):
 if len(data)!=CASES*RECORD:raise RuntimeError('Wrong input dimensions')
 if output is not None and len(output)!=CASES*RESULT:raise RuntimeError('Incomplete original output')
 for p in range(PROBES):
  rows=[bytearray(data[(p*6+j)*RECORD:(p*6+j+1)*RECORD]) for j in range(6)]
  selected=u32(rows[0],40+4392);control=40+4240+selected*16+8
  for row in rows:row[control:control+2]=b'\0\0'
  if any(row[:TABLE_OFFSET]!=rows[0][:TABLE_OFFSET] for row in rows):raise RuntimeError('Controlled matrix differs outside selector/table')
  if rows[0][TABLE_OFFSET:]==rows[1][TABLE_OFFSET:]:raise RuntimeError('Missing different lookup tables')
  if output is not None:
   for mode in range(3):
    a=(p*6+mode*2)*RESULT;b=a+RESULT
    # All shared effects except the real table-tail neighbor are invariant.
    if output[a+4:a+IMAGE]!=output[b+4:b+IMAGE] or output[a+IMAGE:a+IMAGE+32]!=output[b+IMAGE:b+IMAGE+32]:raise RuntimeError('Nonzero pitch depends on changed lookup table')
 image=bytearray(IMAGE);counts={'appended':0,'gated':0,'started':0,'restarted':0,'blocked':0,'rng_steps':0}
 masks=set();pitches=set();velocities=set();metadata=set();sat={'positive':0,'negative':0};per_mode=[0,0,0]
 for c in range(CASES):
  row=data[c*RECORD:(c+1)*RECORD];image=loaded_image(image,row)
  selected=u32(image,4392);loaded=u32(image,4400);note=u32(row,16);explicit=u32(row,28)
  if not explicit or note>=128 or selected>=8 or loaded>=8:raise RuntimeError('Unverified raw argument/slots')
  mode=struct.unpack_from('<h',image,4240+selected*16+8)[0]//4096
  if mode not in (1,2,3) or u32(image,3468) or u32(image,2576)!=1 or u32(image,2568):raise RuntimeError('Unverified mode/initialization/list context')
  if any(u32(image,4528+v*4) for v in range(6)) or u32(image,4560):raise RuntimeError('Auxiliary callback context not absent')
  if bool(image[4])!=bool(u32(row,4)):raise RuntimeError('Gate label differs')
  active=[bool(image[3504+v]) for v in range(6)]
  if image[4]:counts['gated']+=1
  else:
   counts['appended']+=1;per_mode[mode-1]+=1
   masks.add(sum(int(a)<<v for v,a in enumerate(active)));pitches.add(explicit)
   velocities.add(u32(row,20)&65535);metadata.add(row[24])
   for v in range(6 if mode==3 else 1):
    kind='restarted' if active[v] else 'started' if loaded==selected else 'blocked'
    counts[kind]+=1;counts['rng_steps']+=kind!='blocked'
  if output is not None:
   e=output[c*RESULT:(c+1)*RESULT]
   if u32(e,2568)!=(0 if image[4] else 1):raise RuntimeError('Unexpected list count')
   if e[IMAGE:IMAGE+32]!=row[8:40] or e[IMAGE+32:]!=row[TABLE_OFFSET:]:raise RuntimeError('Argument/guards/lookup table changed')
   if not image[4]:
    for v in range(6 if mode==3 else 1):
     if not active[v] and loaded!=selected:continue
     value=u32(e,2580+v*24+12) if active[v] else u32(e,4136+v*4)
     sat['positive']+=value==0x7fffffff;sat['negative']+=value==0x80000000
   image[:]=e[:IMAGE]
 if pitches!=set(PITCHES) or masks!=set(range(64)):raise RuntimeError('Missing pitch patterns/activity masks')
 result={'counts':counts,'appends_per_selector':per_mode,'pitch_patterns':len(pitches),'activity_masks':len(masks),'velocities_low16':sorted(velocities),'metadata':sorted(metadata),'lookup_invariant_pairs':PROBES*3,'selector_triples':PROBES*2}
 if output is not None:result['observed_saturated_outputs']=sat
 return result

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
 templates=ROOT/'research/blackfin';load,save=held.caller();source=(templates/'drwave-empty-explicit-pitch.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'TABLE_OFFSET':str(TABLE_OFFSET),'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
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
 if {k:v for k,v in actual.items() if k!='observed_saturated_outputs'}!=coverage:raise RuntimeError('Coverage preflight differs')
 artifacts={}
 for name,path in [('drwave246-empty-explicit-pitch-input.bin',out/'inputs.bin'),('drwave246-empty-explicit-pitch-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 sources=[Path(__file__).resolve(),templates/'drwave-empty-explicit-pitch.s.in',ROOT/'Source/DrWave246EmptyExplicitPitch.h',ROOT/'Tests/DrWave246EmptyExplicitPitchTests.cpp',ROOT/'scripts/verify-drwave246-held-note-modes.py',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py']
 sources += [ROOT/'Source'/s for s in ['DrWave246Routing.h','DrWave246Kernel.h','DrWave246.h','Voice246Activation.h','Voice246NoteArgument.h','Voice246PolyRelease.h','Envelope246Transition.h','Envelope246.h','Envelope246Tables.h','ReferenceDsp.h']]
 report={'ldr_sha256':expected,'entry':'0x01a84604','end_exclusive':'0x01a84ba0','code_bytes':0x59c,'routine_sha256':hashlib.sha256(image[0xa84604:0xa84ba0]).hexdigest(),'records':CASES,'independent':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,'matrix_records':MATRIX,'pitch_words':[hex(x) for x in PITCHES],'tuning_words':[hex(x) for x in TUNINGS],'coverage':actual,'image_bytes':IMAGE,'input_record_bytes':RECORD,'output_record_bytes':RESULT,'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in sources},'pc_watches':WATCHES,'model':'GNU BF524 with both documented writeback patches; no hardware/timing claim','limits':['Empty entry list and nonzero explicit pitch only, selectors1..3, ready ADSR and absent auxiliary modules. New note index0..127 bounds gain conversion.','Raw arguments, zero count, activity, controls, tuning and complete synthetic lookup table are supplied by own harness. No constructor, original list producer or note lifecycle.', 'Streams retain selected states/objects/list bytes/RNG, but count and activity are explicit every call. No scheduler/cadence inferred.', 'No ADSR/Dr. Wave render or pending-field consumer. Register or pending pitch is not a claimed render buffer writer. CPU/stack/whole-device state is not reconstructed.', 'Firmware, original instructions, disassembly and ELF stay private. Only numeric results and own source are versioned.']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS original empty explicit pitch:',CASES,'records;',actual['counts'],'lookup pairs',actual['lookup_invariant_pairs'])
if __name__=='__main__':main()
