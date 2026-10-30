#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original instrument release dispatcher, empty notification map, no renderer.
PC watches stop before excluded active-map, transport, error and L1 paths.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
INDEPENDENT,DISCARDS,STREAMS,CALLS=1280,80,5,64
CASES=INDEPENDENT+DISCARDS+STREAMS*CALLS
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,False),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),
 (0x018c2180,12,False),(0x01de4c3c,12,False),(0x01aafd5c,136,True),
 (0x01aadb1c,40,True),(0x018b600c,4,True)]
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(r[1] for r in REGIONS[:len(OFF)]))
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=64+IMAGE;RESULT=IMAGE+48
INVALID=[0xffffffff,0x80000000,128,129,255,256,32767,32768,65535,65536,
         0x1007f,0x1000000,0x8000007f,0x7fffffff,0x7fff007f,0xffffff80]
WATCHES=['0x019e2df4','0x01a69360','0x019e0d18','0x01919aa8',
         '0x01a85890','0x01a8589e','0xffa00000,0xffafffff']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def u32(b,o=0):return struct.unpack_from('<I',b,o)[0]
def s16(b,o=0):return struct.unpack_from('<h',b,o)[0]
def put(b,o,v):struct.pack_into('<I',b,o,v&0xffffffff)
def module(name,file):
 spec=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def inputs():
 # Reuse only previously authored valid Core/ADSR/tuning context scaffolding.
 # Its lists, selector, argument and schedule are replaced by this own study.
 base=module('selector_inputs','scripts/verify-voice246-release-selectors.py');old=base.inputs()
 seed=0x44535052
 def rnd():
  nonlocal seed
  seed=(seed*1664525+1013904223)&0xffffffff;return seed
 data=bytearray();metadata=[1,2,127,128,255]
 for c in range(CASES):
  row=old[(c%base.CASES)*base.RECORD:(c%base.CASES+1)*base.RECORD]
  b=bytearray(row[32:32+4464]+struct.pack('<48I',*[rnd() for _ in range(48)]))
  if c<INDEPENDENT:
   mode=c//256;note=(c//2)%128;meta=0 if c%2==0 else metadata[(c//2)%5];count=c%129;reload=1
  elif c<INDEPENDENT+DISCARDS:
   k=c-INDEPENDENT;mode=k//16;note=INVALID[k%16];meta=[0,1,128,255][k%4];count=(k*7)%129;reload=1
  else:
   stream,step=divmod(c-INDEPENDENT-DISCARDS,CALLS);mode=stream
   count=64;note=(step+stream*20)%128;meta=[0,1,2,127,128,255][step%6];reload=int(step==0)
  put(b,0,610832681);put(b,4,0);put(b,2568,count);put(b,OFF[3]+4,0)
  for i in range(128):
   p=8+i*20;put(b,p,(note+1+i%127)%128 if note<128 else i%128)
   put(b,p+4,rnd());b[p+8]=rnd()&255;put(b,p+12,0 if i%3 else [1,0x7fffffff,0x80000000,rnd()][i%4])
   put(b,p+16,(i+c)%8-2)
  if c<INDEPENDENT and count:
   scenario=(c//2)%4
   if scenario==1:put(b,8,note)
   elif scenario==2:
    put(b,8+(count-1)*20,note)
    if count>2:put(b,8,note)
   elif scenario==3:put(b,8+(count//2)*20,note)
  if c>=INDEPENDENT+DISCARDS:
   stream=(c-INDEPENDENT-DISCARDS)//CALLS
   for i in range(128):put(b,8+i*20,(i+stream*20)%128)
  for i in range(8):struct.pack_into('<h',b,OFF[4]+i*16+8,mode*4096+c%4096)
  put(b,OFF[13]+4,0)
  b[OFF[14]+4:OFF[14]+132]=bytes(128) # Retained within streams, never an active map.
  area=bytearray(struct.pack('<8I',*[rnd() for _ in range(8)]))
  put(area,8,note);put(area,12,rnd());area[16]=meta
  put(area,20,0 if c%2 else rnd())
  local=bytearray(struct.pack('<4I',*[rnd() for _ in range(4)]))
  for j in range(9,12):local[j]=area[8+j]^255 # Distinct padding proves field copy.
  data+=struct.pack('<2I',reload,rnd())+area+local+struct.pack('<2I',rnd(),rnd())+b
 if len(data)!=CASES*RECORD:raise RuntimeError('Wrong dispatcher dimensions')
 return data

def eligibility(raw):
 # Validate supplied context before any original instruction is executed.
 state=bytearray(IMAGE)
 for c in range(CASES):
  x=raw[c*RECORD:(c+1)*RECORD]
  for i,(_,n,retain) in enumerate(REGIONS):
   if u32(x) or not retain:state[OFF[i]:OFF[i]+n]=x[64+OFF[i]:64+OFF[i]+n]
  selected=u32(state,OFF[8]);count=u32(state,2568)
  if selected>=8 or count>128 or u32(state,4) or u32(state,OFF[3]+4) or u32(state,OFF[13]+4):raise RuntimeError('Excluded outer context')
  mode=s16(state,OFF[4]+selected*16+8)//4096
  if mode not in range(5) or s16(state,OFF[4]+selected*16+4)<0 or u32(state,OFF[7])!=1:raise RuntimeError('Invalid release context')
  if any(state[OFF[14]+4:OFF[14]+132]):raise RuntimeError('Notification map must stay explicitly empty')
  for j in range(count):
   word=u32(state,8+j*20+16)
   if word<0x80000000 and word>=6:raise RuntimeError('Excluded nonnegative voice index')
   if u32(state,8+j*20+12)==0 and u32(state,8+j*20)>=128:raise RuntimeError('Invalid potential pitch lookup')
 # Original output comparison supplies actual retained mutations; the initial
 # lists here are valid and removals only reduce/compact those same entries.

def caller():
 load=[];save=['P2 = P5;']
 for i,(address,n,retain) in enumerate(REGIONS):
  if retain:load+=['R0 = [P4];','CC = R0 == 0;',f'IF CC JUMP retained_{i};']
  load += [f'P0 = {64+OFF[i]};','P1 = P4 + P0;',f'imm32 P2, {address}',f'P3 = {n//4};',
           f'LSETUP(load_{i},load_end_{i}) LC0 = P3;',f'load_{i}: R0 = [P1++];',f'load_end_{i}: [P2++] = R0;']
  if retain:load+=[f'retained_{i}:']
  save += [f'imm32 P1, {address}',f'P3 = {n//4};',f'LSETUP(save_{i},save_end_{i}) LC0 = P3;',
           f'save_{i}: R0 = [P1++];',f'save_end_{i}: [P2++] = R0;']
 return '\n'.join(load),'\n'.join(save)

def observations(raw,output):
 counts=dict(filtered=0,dispatched=0,metadata_zero=0,metadata_nonzero=0,removed=0,absent_or_disabled=0)
 state=bytearray(IMAGE);notes=[set() for _ in range(5)];lengths=[set() for _ in range(5)]
 for c in range(CASES):
  x=raw[c*RECORD:(c+1)*RECORD];e=output[c*RESULT:(c+1)*RESULT]
  for i,(_,n,retain) in enumerate(REGIONS):
   if u32(x) or not retain:state[OFF[i]:OFF[i]+n]=x[64+OFF[i]:64+OFF[i]+n]
  note=u32(x,16);selected=u32(state,OFF[8]);mode=s16(state,OFF[4]+selected*16+8)//4096
  lengths[mode].add(u32(state,2568));local=bytearray(x[40:56])
  if note>=128:
   counts['filtered']+=1
   if e[:IMAGE]!=state:raise RuntimeError('Filtered note changed context')
  else:
   counts['dispatched']+=1;notes[mode].add(note);counts['metadata_nonzero' if x[24] else 'metadata_zero']+=1
   for i in range(16):
    if i<9 or i>=12:local[i]=x[16+i]
   counts['removed' if u32(e,2568)<u32(state,2568) else 'absent_or_disabled']+=1
  if e[IMAGE:IMAGE+32]!=x[8:40] or e[IMAGE+32:]!=local:raise RuntimeError('Full source argument/guards or local field-copy differs')
  for i in (13,14,15,16):
   if e[OFF[i]:OFF[i]+REGIONS[i][1]]!=state[OFF[i]:OFF[i]+REGIONS[i][1]]:raise RuntimeError('Map/discriminator/transport monitor changed')
  state[:]=e[:IMAGE]
 counts['valid_notes_per_selector']=[len(s) for s in notes];counts['list_lengths_per_selector']=[len(s) for s in lengths]
 return counts

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
 if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
 expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
 if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 LDR is accepted')
 data=inputs();eligibility(data)
 m=module('ldr_map','scripts/map-op1-ldr.py');raw=a.ldr.read_bytes();meta,_=m.inspect(raw)
 base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=address and address+n<=base+len(image) and not flags&0x1000:image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(data)
 templates=ROOT/'research/blackfin';load,save=caller();source=(templates/'voice-release-dispatch.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(command,**kw):return subprocess.run([str(x) for x in command],timeout=180,**kw)
 def build(name,file,linker):
  run([a.assembler,'-I',a.testutils,'-o',out/(name+'.o'),file],check=True)
  run([a.linker,'-T',linker,'-o',out/(name+'.elf'),out/(name+'.o')],check=True)
 build('halfword',templates/'parallel-halfword.s',templates/'flat.ld')
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],check=True,stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 (out/'watch.s').write_text('.text\n.global __start\n__start:\nR0.H = exit_args;\nR0.L = exit_args;\nP0 = 1;\nEXCPT 0;\n.data\n.align 4\nexit_args: .long 0\n')
 build('watch',out/'watch.s',templates/'flat.ld')
 probe_codes=[]
 for opts in [[],['--watch-pc-int','0x1000'],['--watch-pc-int','0x1010','--watch-pc-int','0x1000'],['--watch-pc-int','0x1000,0x1008']]:
  r=run([a.simulator,'--model','bf524',*opts,out/'watch.elf'],capture_output=True);probe_codes.append(r.returncode)
 if probe_codes!=[0,5,5,5]:raise RuntimeError('PC watch positive/negative probes failed')
 build('probe',out/'probe.s',templates/'voice-control-input.ld')
 guards=[arg for address in WATCHES for arg in ['--watch-pc-int',address]]
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
  run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],check=True,stdout=stdout,stderr=stderr)
 output=(out/'output.bin').read_bytes()
 if len(output)!=1028+CASES*RESULT or u32(output)!=0x02ffffe8:raise RuntimeError('Incomplete output or own LINK-frame calibration differs')
 if output[4:516]!=output[-512:] or output[4:516]!=image[0x8c21b0:0x8c23b0]:raise RuntimeError('Original note table copy/preservation differs')
 counts=observations(data,output[516:-512]);artifacts={}
 for name,path in [('voice246-release-dispatch-input.bin',out/'inputs.bin'),('voice246-release-dispatch-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 paths=[Path(__file__).resolve(),templates/'voice-release-dispatch.s.in',ROOT/'Source/Voice246ReleaseDispatch.h',ROOT/'Tests/Voice246ReleaseDispatchTests.cpp',ROOT/'Source/Voice246ReleaseSelectors.h',ROOT/'Source/Voice246PolyRelease.h',ROOT/'scripts/verify-voice246-release-selectors.py']
 report={'ldr_sha256':expected,'dispatcher':'0x01a85840','end_exclusive':'0x01a858d6','code_bytes':150,'routine_sha256':hashlib.sha256(image[0xa85840:0xa858d6]).hexdigest(),
  'notification':'0x019e2dc0','notification_end_exclusive':'0x019e2e62','release':'0x01a85058','records':CASES,'independent_valid':INDEPENDENT,'independent_filtered':DISCARDS,'streams':STREAMS,'calls_per_stream':CALLS,'coverage':counts,
  'image_bytes':IMAGE,'input_record_bytes':RECORD,'output_record_bytes':RESULT,'caller_stack_pointer':'0x03000000','own_calibrated_local_argument':'0x02ffffe8','regions':[{'address':hex(addr),'bytes':n,'retained':retain} for addr,n,retain in REGIONS],
  'pc_watches':WATCHES,'pc_watch_probe_exit_codes':probe_codes,'pc_watch_original_run_exit_code':0,
  'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in paths},
  'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},
  'model':'GNU BF524 with both documented writeback patches, not hardware timing',
  'limitations':['Instrument discriminator zero only; explicit empty 128-byte notification map, initialized Core/ADSR pools, outer flags/mode zero and selectors 0..4.',
   'Transport/active-map/error/L1 PCs are execution-guarded. Map producers and active pending output are not recovered or executed.',
   'No constructor, allocator, poly-full caller/helper, render, output MIDI, scheduler or plugin integration is introduced. Call order, controls, list seeds and tuning coefficient remain explicit.',
   'Source argument and local field copy are compared separately; frame position is own harness calibration, not an original fixed address. No semantic return is assigned to the dispatcher.',
   'Firmware, original instructions, disassembly and ELF remain private.']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original bounded release dispatch:',counts)
if __name__=='__main__':main()
