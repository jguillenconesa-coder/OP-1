#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Explicit constructor/on/off/ADSR sequences with state and list retained.
No pending-field consumer, original event clock or pitch-buffer producer.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=1536
WATCHES=['0x01a84690','0x019e2df4','0x01a69360','0x019e0d18','0x01919aa8','0x01a85890','0x01a8589e','0xffa00000,0xffafffff']
# Address, bytes, retain across each of the twelve 128-event streams.
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,True),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),(0x018c2180,12,False),(0x01de4c3c,12,False),(0x01dfe4d8,48,True),(0x01aaad68,32,False),(0x01aaac40,12,False),(0x01aafd5c,136,True),(0x01aadb1c,40,True),(0x018b600c,4,True)]
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=40+IMAGE+272;RESULT=IMAGE*2+364
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
 spec=importlib.util.spec_from_file_location('first',ROOT/'scripts/verify-drwave246-poly-first-free.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 old=m.inputs();data=bytearray()
 for c in range(CASES):
  row=old[(c%m.CASES)*m.RECORD:(c%m.CASES+1)*m.RECORD];header=bytearray(row[:40]);image=bytearray(row[40:40+m.IMAGE])+bytearray(180)
  stream=c//128;step=c%8;cycle=(c%128)//8;mode=1+stream//4
  struct.pack_into('<I',header,0,1 if c%128==0 else 0)
  action=0 if step in [0,1,2,4] else 1
  struct.pack_into('<I',header,12,action)
  a=((stream%4)*16+cycle)%128;b=(a+37)%128;d=(a+79)%128
  target=[a,a,b,a,d,d,b,(a+113)%128][step]
  struct.pack_into('<I',header,16,(target-53)&0xffffffff)
  struct.pack_into('<I',header,20,((c*0x9e37)<<16 | [0,1,127,128,255,32767,32768,65535][c%8])&0xffffffff)
  struct.pack_into('<I',header,4,(c//8)%6)
  struct.pack_into('<2i',header,32,c%8,0)
  struct.pack_into('<8I',image,4484,*([0]*8));struct.pack_into('<I',image,4468,0)
  for i in range(8):struct.pack_into('<h',image,4240+i*16+8,(mode<<12)|(c%4096))
  struct.pack_into('<I',image,2568,0)
  image[4]=0 # Gate supplied at stream reset only, retained thereafter.
  for i in range(6):image[3504+i]=[0,1,128,255][(stream+i)%4]
  # Explicit independent sound selection/tuning/controls; retained voice state.
  selected=struct.unpack_from('<I',image,4392)[0]
  if stream%4!=3:struct.pack_into('<I',image,4400,selected)
  # Notification map empty, with nonzero neighbors and transport sentinels.
  for i in range(180):image[4568+i]=(c*19+i*53+7)&255
  image[4572:4700]=bytes(128)
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
 source=(templates/'drwave-note-lifecycle.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'BEFORE':save.replace('save_','before_'),'AFTER':save.replace('save_','after_'),'COEFFICIENTS':conversion,'RECORD':str(RECORD),'IMAGE':str(IMAGE),'BYTES':str(CASES*RESULT),'ACTIVITY':''}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 # Prove PC watches stop a program of our own before trusting negative guards.
 (out/'watch.s').write_text('.text\n.global __start\n__start:\nR0.H = exit_args;\nR0.L = exit_args;\nP0 = 1;\nEXCPT 0;\n.data\n.align 4\nexit_args: .long 0\n')
 run([a.assembler,'-I',a.testutils,'-o',out/'watch.o',out/'watch.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'watch.elf',out/'watch.o'])
 codes=[]
 for opts in [[],['--watch-pc-int','0x1000'],['--watch-pc-int','0x1010','--watch-pc-int','0x1000'],['--watch-pc-int','0x1000,0x1008']]:
  codes.append(subprocess.run([str(a.simulator),'--model','bf524',*opts,str(out/'watch.elf')],capture_output=True,timeout=180).returncode)
 if codes!=[0,5,5,5]:raise RuntimeError('PC watch probes failed')
 guards=[arg for address in WATCHES for arg in ['--watch-pc-int',address]]
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],stdout=stdout,stderr=stderr)
 if (out/'output.bin').stat().st_size!=CASES*RESULT+1024:raise RuntimeError('Incomplete output')
 counts={'on':0,'off':0,'appended':0,'duplicates':0,'removed':0,'absent':0,'empty_on':0,'held_on':0};all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Note table changed')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes();previous=0
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  if struct.unpack_from('<I',x)[0]:previous=0
  after=struct.unpack_from('<I',e,2568)[0];action=struct.unpack_from('<I',x,12)[0]
  counts['on' if action==0 else 'off']+=1
  if not action:
   counts['empty_on' if previous==0 else 'held_on']+=1
   counts['appended' if after==previous+1 else 'duplicates']+=1
  else:counts['removed' if after+1==previous else 'absent']+=1
  previous=after
 if previous!=0:raise RuntimeError('Final list not empty')
 artifacts={}
 for name,path in [('drwave246-note-lifecycle-input.bin',out/'inputs.bin'),('drwave246-note-lifecycle-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'entries':['0x01a84604','0x01a85840'],'context':'Explicit event order with retained list, voice state and RNG, selectors 1-3, constructor pitch zero, empty notification map, null auxiliaries and initialized ADSR.','records':CASES,'streams':12,'blocks_per_stream':128,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with both documented writeback patches; not hardware or pipeline validation','limitations':['Harness supplies event order, controls, tuning, selected/loaded sound and explicit ADSR render voice/volume. No recovered MIDI dispatcher cadence, scheduler or realtime host integration.','List count, activity, pitch, velocity and state carry between events without per-call overrides. Initial activity is explicit and release does not invent activity clearing.','No pending-field consumer, pitch-buffer producer, Dr. Wave audio or notification transport. Map is initialized empty and retained.']}
 paths=[Path(__file__).resolve(),templates/'drwave-note-lifecycle.s.in',ROOT/'Source/DrWave246NoteModes.h',ROOT/'Source/DrWave246HeldNoteModes.h',ROOT/'Source/Voice246ReleaseDispatch.h',ROOT/'Tests/DrWave246NoteLifecycleTests.cpp',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py']
 paths+=list((ROOT/'Source').glob('*246*.h'))
 report['pc_watches']=WATCHES;report['pc_watch_probe_exit_codes']=codes;report['pc_watch_original_run_exit_code']=0
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in paths}
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original note lifecycle:',CASES,counts)
if __name__=='__main__':main()
