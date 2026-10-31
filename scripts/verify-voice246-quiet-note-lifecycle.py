#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Explicit constructor/on/off/ADSR sequences with state and list retained.
No pending-field consumer, original event clock or pitch-buffer producer.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=1536
WATCHES=['0x01a84690','0x019e2ebe','0x019e2f2c','0x019e2e00','0x019e0d18','0x01919aa8','0x01a857d0','0x01a85890','0x01a8589e','0xffa00000,0xffafffff']
# Address, bytes, retain across each of the twelve 128-event streams.
REGIONS=[(0x01dfe72c,2576,True),(0xff8000d0,664,True),(0xffb00000,224,True),
 (0xff801034,776,True),(0x01b60ef0,128,False),(0xff802bc8,8,True),
 (0x01b60534,8,True),(0x01b6053c,8,False),(0xff8016d4,8,False),
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),(0x018c2180,12,False),(0x01de4c3c,12,False),(0x01dfe4d8,48,True),(0x01aaad68,32,False),(0x01aaac40,12,False),(0x01aafd5c,136,True),(0x01aadb1c,40,True),(0x018b600c,4,True),(0x01dfe038,12,True)]
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=40+IMAGE+272;RESULT=IMAGE*2+380
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
 spec=importlib.util.spec_from_file_location('life',ROOT/'scripts/verify-drwave246-note-lifecycle.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 old=m.inputs();data=bytearray()
 for c in range(CASES):
  row=old[c*m.RECORD:(c+1)*m.RECORD];header=bytearray(row[:40]);image=bytearray(row[40:40+m.IMAGE])+bytearray(struct.pack('<3I',0xa55a5aa5,0,0x12345678))
  if c%16==0:struct.pack_into('<I',header,20,0)
  elif c%16==2:struct.pack_into('<I',header,20,0xffffffff)
  image[4]=1 if (c//128)%4==3 and struct.unpack_from('<I',header,12)[0]==0 else 0 # Off requires gate zero.
  image[4572:4700]=bytes([0,1,128,255][(j+c//128)%4] for j in range(128))
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
 source=(templates/'voice-quiet-note-lifecycle.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'BEFORE':save.replace('save_','before_'),'AFTER':save.replace('save_','after_'),'COEFFICIENTS':conversion,'RECORD':str(RECORD),'IMAGE':str(IMAGE),'BYTES':str(CASES*RESULT),'ACTIVITY':''}.items():source=source.replace('@'+k+'@',v)
 (out/'probe.s').write_text(source)
 def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
 run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
 with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:run([a.simulator,'--model','bf524',out/'halfword.elf'],stdout=stdout,stderr=stderr)
 if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):raise RuntimeError('Apply both documented GNU writeback patches')
 # Own LINK frame calibration and positive/negative PC watch checks.
 (out/'frame.s').write_text('.include "testutils.inc"\nstart\nimm32 SP, 0x03000000\nCALL frame;\nP1.H = value;\nP1.L = value;\n[P1] = R0;\nR0.H = args;\nR0.L = args;\nP0 = 5;\nEXCPT 0;\nexit 0\nframe:\nLINK 0x10;\nR0 = FP;\nR0 += -16;\nUNLINK;\nRTS;\n.data\n.align 4\nargs: .long 1,value,4\nvalue: .long 0\n')
 run([a.assembler,'-I',a.testutils,'-o',out/'frame.o',out/'frame.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'frame.elf',out/'frame.o'])
 with (out/'frame.bin').open('wb') as stdout:run([a.simulator,'--model','bf524',out/'frame.elf'],stdout=stdout)
 if (out/'frame.bin').read_bytes()!=struct.pack('<I',0x02ffffe8):raise RuntimeError('Own local frame calibration differs')
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
 counts={'on':0,'off':0,'appended':0,'gated':0,'duplicates':0,'removed':0,'absent':0,'empty_on':0,'held_on':0,'map_changes':0};all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Note table changed')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes();previous=0;previous_map=None
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  if struct.unpack_from('<I',x)[0]:previous=0;previous_map=x[40+4572:40+4700]
  after=struct.unpack_from('<I',e,2568)[0];action=struct.unpack_from('<I',x,12)[0]
  counts['on' if action==0 else 'off']+=1
  if not action:
   counts['empty_on' if previous==0 else 'held_on']+=1
   counts['gated' if x[44] else 'appended' if after==previous+1 else 'duplicates']+=1
  else:counts['removed' if after+1==previous else 'absent']+=1
  counts['map_changes']+=sum(a!=b for a,b in zip(previous_map,e[4572:4700]));previous_map=e[4572:4700]
  if e[4704:4748]!=x[40+4704:40+4748] and struct.unpack_from('<I',x)[0]:raise RuntimeError('Transport sentinel changed')
  previous=after
 if previous!=0:raise RuntimeError('Final list not empty')
 artifacts={}
 for name,path in [('voice246-quiet-note-lifecycle-input.bin',out/'inputs.bin'),('voice246-quiet-note-lifecycle-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'entries':['0x01a85750','0x01a85840'],'notification_entries':['0x019e2e64','0x019e2dc0'],'output_getter':'0x01a69360..0x01a6936c','output_word':'0x01dfe03c','explicit_output_word':0,'context':'Original constructor metadata 1, full outer on/off, initialized pools, selectors 1-3, output word zero. No notification transport or erased metadata.','records':CASES,'streams':12,'blocks_per_stream':128,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with both documented writeback patches; not hardware or pipeline validation','limitations':['Event order, controls/tuning/selection and ADSR render voice/volume explicit; no recovered scheduler or host integration.','List, activity, registers, pools, RNG, notification map, output word and monitor sentinels retained within each sequence; inner gate explicit per event and zero for off.','Output word zero is a supplied precondition, not a factory default. No packet preparation, actual transport, pending-field consumer or pitch-buffer producer.']}
 paths=[Path(__file__).resolve(),templates/'voice-quiet-note-lifecycle.s.in',ROOT/'Source/DrWave246NoteModes.h',ROOT/'Source/DrWave246HeldNoteModes.h',ROOT/'Source/Voice246QuietNoteDispatch.h',ROOT/'Source/Voice246NoteDispatch.h',ROOT/'Tests/Voice246QuietNoteLifecycleTests.cpp',ROOT/'scripts/verify-drwave246-note-lifecycle.py',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py']
 paths+=list((ROOT/'Source').glob('*246*.h'))
 report['pc_watches']=WATCHES;report['pc_watch_probe_exit_codes']=codes;report['pc_watch_original_run_exit_code']=0
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in paths}
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original quiet note lifecycle:',CASES,counts)
if __name__=='__main__':main()
