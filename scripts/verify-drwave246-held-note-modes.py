#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Complete original note caller, selectors 1..3 with NONEMPTY entry lists only.
Own arguments/order; no empty-list branch, ADSR render, pending-field consumer,
encrypted code, scheduler or plugin integration.
"""
import argparse, hashlib, importlib.util, json, struct, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
INDEPENDENT,STREAMS,CALLS=1152,6,64
CASES=INDEPENDENT+STREAMS*CALLS
# Same bounded monitor addresses as the merged poly work; own format/caller.
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,True),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),
 (0x018c2180,12,False),(0x01de4c3c,12,False),(0x01dfe4d8,48,True),
 (0x01aaad68,32,False),(0x01aaac40,12,False)]
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
IMAGE=sum(n for _,n,_ in REGIONS)
RECORD,RESULT=40+IMAGE,IMAGE+32
WATCHES=['0x01a8487e','0x01a84690','0x01919aa8','0xffa00000,0xffafffff']

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path)
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def u32(data,p=0):return struct.unpack_from('<I',data,p)[0]
def put(data,p,v):struct.pack_into('<I',data,p,v&0xffffffff)

def inputs():
 # Reuse only a merged main generator for valid ready/pointer/vtable context.
 old=module('merged_free','scripts/verify-drwave246-poly-first-free.py');base=old.inputs()
 assert old.IMAGE==IMAGE
 data=bytearray();seed=0x48454c44
 def rnd():
  nonlocal seed
  seed=(seed*1664525+1013904223)&0xffffffff;return seed
 for c in range(CASES):
  # First 768 records are 256 controlled triples: ONLY selected mode differs.
  pattern=c%256 if c<768 else c
  seed=(0x48454c44^(pattern*0x9e3779b9))&0xffffffff
  image=bytearray(base[(pattern%768)*old.RECORD+40:(pattern%768)*old.RECORD+40+IMAGE])
  header=bytearray(struct.pack('<10I',*[rnd() for _ in range(10)]))
  if c<768:selector=c//256+1;kind=0;note=c%128;explicit=c%256>=128;reset=1
  elif c<1152:
   selector=(c-768)%3+1;kind=1 if c<960 else 2
   note=[0,127,128,0xffffffff,0x80000000,0x10000,0x100007f,0x7fffffff][c%8]
   explicit=c%2;reset=1
  else:
   stream,step=divmod(c-INDEPENDENT,CALLS)
   selector=stream%3+1 if stream<3 else step%3+1
   kind=2 if step%8==1 else 1 if step%8==2 else 0
   note=step-1 if kind==2 else step
   explicit=(step+stream)%2;reset=int(step==0)
  put(header,0,reset);put(header,4,kind)
  put(header,16,note)
  velocity=[0,1,127,128,255,32767,32768,65535][(pattern//3)%8]
  put(header,20,(rnd()&0xffff0000)|velocity)
  header[24]=[0,1,128,255][(pattern//5)%4]
  pitch=[1,0xffffffff,0x7fffffff,0x80000000,0x40000000,0x10001,0x12345678][(pattern//7)%7]
  put(header,28,pitch if explicit else 0)
  selected=pattern%8;put(image,4392,selected)
  struct.pack_into('<h',image,4240+selected*16+8,selector*4096+[0,1,2048,4095][pattern%4])
  put(image,4,(rnd()&0xffffff00)|([1,128,255][c%3] if kind==1 else 0))
  put(image,3468,0) # Outer mode zero, not the per-sound selector.
  put(image,2576,1) # Six ADSR states are already initialized.
  for j in range(6):
   put(image,2580+j*24,[0,1,3,6,7,8,0xffffffff][(pattern+j)%7])
   put(image,2584+j*24,[0,1,0x7fffffff,0x80000000,0xffffffff][(pattern+j)%5])
   activity=(pattern//4)%3
   image[3504+j]=0 if activity==0 else [1,128,255][(pattern+j)%3] if activity==1 else [0,1,128,255][(pattern+j)%4]
  tuning=[0,1,0x7fffffff,0x80000000,0xffffffff,0x40000000,0x12345678][(pattern//11)%7]
  put(image,4444,tuning);put(image,4456,1200)
  # No auxiliary callbacks. Dr. Wave objects keep main's valid vtables.
  for j in range(6):put(image,4528+j*4,0)
  put(image,4560,0)
  if reset:
   n=(pattern%127+1 if kind==0 else c%128+1) if c<INDEPENDENT else 17
   put(image,2568,n)
   for j in range(128):
    put(image,8+j*20,0x10000000+j)
    put(image,8+j*20+16,[0xfffffffe,0xffffffff,0,1,5,0x10000][(pattern+j)%6])
    image[8+j*20+8]=[0,1,128,255][(pattern+j)%4]
   if kind==2:
    where=[0,n//2,n-1][(c//3)%3];put(image,8+where*20,note)
    if n>1 and c%2==0:put(image,8+(n-1)*20,note)
   elif n:put(image,8,0x10000000|note) # Full-word, not low-byte equality.
  data+=header+image
 return data

def loaded_image(previous,row):
 image=bytearray(previous)
 for i,(_,n,retain) in enumerate(REGIONS):
  if u32(row) or not retain:image[OFF[i]:OFF[i]+n]=row[40+OFF[i]:40+OFF[i]+n]
 # Gate and activity are explicitly supplied every call, without replacing list.
 image[4]=row[44];image[3504:3510]=row[3544:3550]
 return image

def observations(data,output=None):
 if len(data)!=CASES*RECORD:raise RuntimeError('Invalid input dimensions')
 for c in range(256):
  triple=[bytearray(data[(c+s*256)*RECORD:(c+s*256+1)*RECORD]) for s in range(3)]
  selected=u32(triple[0],40+4392);pos=40+4240+selected*16+8
  for row in triple:row[pos:pos+2]=b'\0\0'
  if triple[0]!=triple[1] or triple[0]!=triple[2]:raise RuntimeError('Controlled selector triples differ beyond selector word')
 image=bytearray(IMAGE);counts={'appended':0,'gated':0,'duplicate':0}
 coverage={'selectors':set(),'accepted_notes':{s:set() for s in range(1,4)},'lengths':set(),
           'metadata':set(),'pitch_routes':set(),'rng_steps':0,'all_inactive':0,'all_active':0,
           'loaded_match':0,'loaded_mismatch':0,'selector_appends':[0,0,0]}
 if output is not None and len(output)!=CASES*RESULT+1024:raise RuntimeError('Incomplete original output')
 for c in range(CASES):
  row=data[c*RECORD:(c+1)*RECORD];image=loaded_image(image,row)
  n=u32(image,2568);note=u32(row,16);gate=image[4]
  if not 1<=n<=128:raise RuntimeError('Entry list must ALWAYS be nonempty and within capacity')
  selected=u32(image,4392)
  if selected>=8:raise RuntimeError('Unsupported selected slot')
  mode=struct.unpack_from('<h',image,4240+selected*16+8)[0]//4096
  if mode not in (1,2,3) or u32(image,3468)!=0:raise RuntimeError('Unsupported mode context')
  if u32(image,2576)!=1 or any(u32(image,4528+j*4) for j in range(6)) or u32(image,4560):raise RuntimeError('Dependencies not bounded')
  duplicate=any(u32(image,8+j*20)==note for j in range(n))
  kind='gated' if gate else 'duplicate' if duplicate else 'appended'
  if kind=='appended' and (n==128 or note>=128):raise RuntimeError('Would reach excluded capacity/lookup/gain context')
  if {'appended':0,'gated':1,'duplicate':2}[kind]!=u32(row,4):raise RuntimeError('Generator classification differs')
  counts[kind]+=1;coverage['selectors'].add(mode);coverage['lengths'].add(n)
  if kind=='appended':
   coverage['accepted_notes'][mode].add(note);coverage['metadata'].add(row[24])
   coverage['pitch_routes'].add(bool(u32(row,28)));coverage['rng_steps']+=6 if mode==3 else 1
   coverage['selector_appends'][mode-1]+=1
   active=image[3504:3510];coverage['all_inactive']+=not any(active);coverage['all_active']+=all(active)
   coverage['loaded_match' if u32(image,4400)==selected else 'loaded_mismatch']+=1
  if output is not None:
   e=output[512+c*RESULT:512+(c+1)*RESULT]
   if e[IMAGE:]!=row[8:40]:raise RuntimeError('Argument/guards changed')
   if u32(e,2568)!=n+(kind=='appended'):raise RuntimeError('List count transition differs')
   image[:]=e[:IMAGE]
  elif kind=='appended':
   p=8+n*20;image[p:p+9]=row[16:25];image[p+12:p+16]=row[28:32];put(image,p+16,0xfffffffe);put(image,2568,n+1)
 if any(len(v)!=128 for v in coverage['accepted_notes'].values()):raise RuntimeError('Missing accepted notes per selector')
 return {'counts':counts,'selectors':sorted(coverage['selectors']),
         'accepted_notes_per_selector':{str(k):len(v) for k,v in coverage['accepted_notes'].items()},
         'entry_lengths':sorted(coverage['lengths']),'metadata':sorted(coverage['metadata']),
         'explicit_and_lookup_pitch':sorted(coverage['pitch_routes']),'rng_steps':coverage['rng_steps'],
         'controlled_selector_triples':256,'selector_appends':coverage['selector_appends'],
         'accepted_all_inactive':coverage['all_inactive'],'accepted_all_active':coverage['all_active'],
         'accepted_loaded_match':coverage['loaded_match'],'accepted_loaded_mismatch':coverage['loaded_mismatch']}

def caller():
 load='';save=''
 for i,(addr,n,retain) in enumerate(REGIONS):
  if retain:load+=f'R0 = [P4];\nCC = R0 == 0;\nIF CC JUMP skip_{i};\n'
  load+=f'P0 = {40+OFF[i]};\nP1 = P4 + P0;\nimm32 P2, {addr}\nP3 = {n//4};\nLSETUP(load_{i},load_end_{i}) LC0 = P3;\nload_{i}: R0 = [P1++];\nload_end_{i}: [P2++] = R0;\n'
  if retain:load+=f'skip_{i}:\n'
  save+=f'imm32 P1, {addr}\nP2 = P5;\nP3 = {n//4};\nLSETUP(save_{i},save_end_{i}) LC0 = P3;\nsave_{i}: R0 = [P1++];\nsave_end_{i}: [P2++] = R0;\nP5 = P2;\n'
 load+='R0 = B[P4+44] (Z);\nimm32 P0, 0x01dfe730\nB[P0] = R0;\nP0 = 3544;\nP1 = P4 + P0;\nimm32 P2, 0xff80105c\nP3 = 6;\nLSETUP(active,active_end) LC0 = P3;\nactive: R0 = B[P1++] (Z);\nactive_end: B[P2++] = R0;\n'
 return load,save

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
 if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
 expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
 if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 LDR supported')
 data=inputs();coverage=observations(data)
 mapper=module('mapper','scripts/map-op1-ldr.py');raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw)
 base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,addr,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=addr and addr+n<=base+len(image) and not flags&0x1000:image[addr-base:addr-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(data)
 templates=ROOT/'research/blackfin';load,save=caller();source=(templates/'drwave-held-note-modes.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 guards=[x for pc in WATCHES for x in ['--watch-pc-int',pc]]
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],stdout=stdout,stderr=stderr)
 output=(out/'output.bin').read_bytes();actual=observations(data,output)
 if actual!=coverage:raise RuntimeError('Observed coverage differs from preflight')
 if output[:512]!=output[-512:] or output[:512]!=image[0x8c21b0:0x8c23b0]:raise RuntimeError('Note table changed')
 artifacts={}
 for name,path in [('drwave246-held-note-modes-input.bin',out/'inputs.bin'),('drwave246-held-note-modes-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 sources=[Path(__file__).resolve(),templates/'drwave-held-note-modes.s.in',ROOT/'Source/DrWave246HeldNoteModes.h',ROOT/'Tests/DrWave246HeldNoteModesTests.cpp',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py',ROOT/'Source/Voice246Activation.h',ROOT/'Source/Voice246NoteArgument.h',ROOT/'Source/Voice246PolyRelease.h',ROOT/'Source/Voice246NoteTable.h',ROOT/'Source/Envelope246Transition.h',ROOT/'Source/Envelope246.h',ROOT/'Source/Envelope246Tables.h',ROOT/'Source/ReferenceDsp.h',templates/'voice-control-input.ld',templates/'parallel-halfword.s']
 report={'ldr_sha256':expected,'entry':'0x01a84604','end_exclusive':'0x01a84ba0','code_bytes':0x59c,'routine_sha256':hashlib.sha256(image[0xa84604:0xa84ba0]).hexdigest(),'held_branch':'0x01a84a70','records':CASES,'independent':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,'coverage':coverage,'image_bytes':IMAGE,'input_record_bytes':RECORD,'result_record_bytes':RESULT,'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in sources},'pc_watches':WATCHES,'model':'GNU BF524 with both documented writeback patches; no hardware/timing claim','limits':['Every entry list is nonempty. No empty-list branch, selectors 0/4, present auxiliary modules, lazy ADSR initialization, capacity/assert path or invalid lookup/gain input.', 'Arguments are raw explicit 16-byte inputs with guards; constructor, adjustment setters, transport and dispatcher are not part of this oracle.', 'List, ADSR, objects, registers, RNG and selected regions carry between stream calls. Gate/activity and other nonretained controls are explicit per-call inputs. No original scheduler/clock inferred.', 'No ADSR/Dr. Wave render or pending-field consumer; no invented L1 handoff or pitch buffer write. Selected RAM regions are monitored, not complete CPU/stack/machine state.', 'Firmware, original instructions, disassembly and ELF remain private.']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS original held-note modes:',CASES,'records;',coverage['counts'],'full image',IMAGE)
if __name__=='__main__':main()
