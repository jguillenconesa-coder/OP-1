#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original note caller: empty-list selectors 1-3 and explicit ADSR render.
No held-note execution, pending-field consumer, clock or pitch-buffer producer.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=768
WATCHES=['0x01a84a70','0x01a84690','0x01919aa8','0xffa00000,0xffafffff']
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
 spec=importlib.util.spec_from_file_location('first',ROOT/'scripts/verify-drwave246-poly-first-free.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 old=m.inputs();data=bytearray()
 for c in range(CASES):
  row=old[c*m.RECORD:(c+1)*m.RECORD];header=bytearray(row[:40]);image=bytearray(row[40:40+IMAGE])
  mode=1+(c//128)%3 if c<384 else 4 if c<448 else 1+c%3
  struct.pack_into('<I',header,0,1 if c<512 or (c-512)%64==0 else 0)
  struct.pack_into('<I',header,12,mode)
  selected=struct.unpack_from('<I',image,4392)[0]
  if c<384:struct.pack_into('<I',image,4400,selected)
  for i in range(8):struct.pack_into('<h',image,4240+i*16+8,(mode<<12)|(c%4096))
  image[4]=1 if 448<=c<512 or c>=512 and c%4==3 else 0
  mask=c%64
  for i in range(6):image[3504+i]=[1,128,255][(c+i)%3] if mask&(1<<i) else 0
  struct.pack_into('<I',image,2568,0) # Explicit empty list at EACH invocation.
  struct.pack_into('<I',header,4,c%6)
  # All 128 constructed notes for each selector before the streams.
  target=c%128
  table=list(struct.unpack_from('<8i',image,4484));slot,value=struct.unpack_from('<2i',header,32);table[slot]=value
  disc=struct.unpack_from('<I',image,4468)[0]
  struct.pack_into('<I',header,16,(target-53-(0 if disc==1 else table[selected]*12))&0xffffffff)
  # Streams use the constructor bypass: table still tested, note stays explicit.
  if c>=512:
   struct.pack_into('<I',image,4468,1);struct.pack_into('<I',header,16,(target-53)&0xffffffff)
  struct.pack_into('<I',header,20,((c*0x9e37)<<16 | [0,1,127,128,255,32767,32768,65535][c%8])&0xffffffff)
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
 source=(templates/'drwave-note-modes.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'BEFORE':save.replace('save_','before_'),'AFTER':save.replace('save_','after_'),'COEFFICIENTS':conversion,'RECORD':str(RECORD),'IMAGE':str(IMAGE),'BYTES':str(CASES*RESULT),'ACTIVITY':'P0 = 3544;\nP1 = P4 + P0;\nimm32 P2, 0xff80105c\nP3 = 6;\nLSETUP(activity_in,activity_in_end) LC0 = P3;\nactivity_in: R0 = B[P1++] (Z);\nactivity_in_end: B[P2++] = R0;'}.items():source=source.replace('@'+k+'@',v)
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
 counts={'appended':0,'gated':0,'selector4':0};all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Note table changed')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes()
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  mode=struct.unpack_from('<I',x,12)[0]
  counts['gated' if x[44] else 'selector4' if mode==4 else 'appended']+=1
  if struct.unpack_from('<I',e,2568)[0]!=(0 if x[44] or mode==4 else 1):raise RuntimeError('Unexpected list count')
 artifacts={}
 for name,path in [('drwave246-note-modes-input.bin',out/'inputs.bin'),('drwave246-note-modes-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'entry':'0x01a84604','end_exclusive':'0x01a84ba0','context':'Selectors 1-3 with explicitly empty list each call; selector 4 and byte gate returns. No explicit pitch override; constructor emits zero. Null auxiliary modules, initialized ADSR, outer mode zero.','records':CASES,'independent':512,'streams':4,'blocks_per_stream':64,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with both documented writeback patches; not hardware or pipeline validation','limitations':['Empty list count and six activity bytes explicitly supplied every call, including streams; no original scheduler or activity producer.','No held-note branches, pending-field consumer, pitch-buffer producer, Dr. Wave audio, plugin or host integration.','Original constructor emits explicit pitch zero. Lookup table and explicit tuning coefficient only; other argument forms excluded.']}
 paths=[Path(__file__).resolve(),templates/'drwave-note-modes.s.in',ROOT/'Source/DrWave246NoteModes.h',ROOT/'Tests/DrWave246NoteModesTests.cpp',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py']
 paths+=list((ROOT/'Source').glob('*246*.h'))
 report['pc_watches']=WATCHES;report['pc_watch_probe_exit_codes']=codes;report['pc_watch_original_run_exit_code']=0
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in paths}
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original empty-list note modes:',CASES,counts)
if __name__=='__main__':main()
