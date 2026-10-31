#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Complete outer note dispatcher, discriminator zero, metadata/pitch zero.
Explicit ready pools, controls and arguments; no notification or assert paths.
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
RECORD,RESULT=40+IMAGE,IMAGE+48
WATCHES=['0x01a84690','0x019e2e64','0x01a857d0','0x01919aa8','0xffa00000,0xffafffff']

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path)
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def u32(data,p=0):return struct.unpack_from('<I',data,p)[0]
def put(data,p,v):struct.pack_into('<I',data,p,v&0xffffffff)

def inputs():
 old=module('held','scripts/verify-drwave246-held-note-modes.py');data=bytearray(old.inputs())
 for c in range(CASES):
  p=c*RECORD;note=c%128 if c<INDEPENDENT else u32(data,p+16)
  put(data,p+12,[0,1,0x7fffffff,0x80000000,0xffffffff][c%5])
  put(data,p+16,note);data[p+24]=0;put(data,p+28,0)
  put(data,p+40+4468,0) # Instrument discriminator.
  if c%5==0:put(data,p+20,0xffffffff) # Full-word sentinel, not any low -1.
  elif c%5==1:put(data,p+20,0x1234ffff)
  if c<768 and c%256<128:put(data,p+40+2568,0)
  if 960<=c<1152:
   n=u32(data,p+40+2568);put(data,p+40+8+(c%n)*20,note)
  # Payload neighbor byte gets set to 1 even when the inner gate suppresses on.
  data[p+40+4481]=[0,1,128,255][c%4]
 return data

def observations(data,output):
 if len(output)!=CASES*RESULT+1024:raise RuntimeError('Incomplete output')
 counts={'appended':0,'gated':0,'duplicates':0,'velocity_sentinel':0,'empty_entry':0}
 image=bytearray(IMAGE)
 for c in range(CASES):
  row=data[c*RECORD:(c+1)*RECORD];e=output[512+c*RESULT:512+(c+1)*RESULT]
  for i,(_,n,retain) in enumerate(REGIONS):
   if u32(row) or not retain:image[OFF[i]:OFF[i]+n]=row[40+OFF[i]:40+OFF[i]+n]
  image[4]=row[44];image[3504:3510]=row[3544:3550]
  n=u32(image,2568);after=u32(e,2568)
  counts['empty_entry']+=n==0
  counts['gated' if image[4] else 'appended' if after==n+1 else 'duplicates']+=1
  counts['velocity_sentinel']+=u32(row,20)==0xffffffff
  if e[4481]!=1:raise RuntimeError('Note flag not set')
  if e[IMAGE+32:IMAGE+48][9:12]!=bytes.fromhex('776655'):raise RuntimeError('Local padding differs')
  image[:]=e[:IMAGE]
 return counts

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
 data=inputs()
 mapper=module('mapper','scripts/map-op1-ldr.py');raw=a.ldr.read_bytes();meta,_=mapper.inspect(raw)
 base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
 while cursor<len(raw):
  flags,addr,n,arg=struct.unpack_from('<4I',raw,cursor)
  if base<=addr and addr+n<=base+len(image) and not flags&0x1000:image[addr-base:addr-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
  cursor+=16+(0 if flags&0x100 else n)
 out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(data)
 templates=ROOT/'research/blackfin';load,save=caller();source=(templates/'voice-note-dispatch.s.in').read_text()
 for k,v in {'WORK':out.as_posix(),'LOAD':load,'SAVE':save,'RECORD':str(RECORD),'BYTES':str(CASES*RESULT)}.items():source=source.replace('@'+k+'@',v)
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
 (out/'watch.s').write_text('.text\n.global __start\n__start:\nR0.H = exit_args;\nR0.L = exit_args;\nP0 = 1;\nEXCPT 0;\n.data\n.align 4\nexit_args: .long 0\n')
 run([a.assembler,'-I',a.testutils,'-o',out/'watch.o',out/'watch.s'])
 run([a.linker,'-T',templates/'flat.ld','-o',out/'watch.elf',out/'watch.o'])
 codes=[]
 for opts in [[],['--watch-pc-int','0x1000'],['--watch-pc-int','0x1010','--watch-pc-int','0x1000'],['--watch-pc-int','0x1000,0x1008']]:
  codes.append(subprocess.run([str(a.simulator),'--model','bf524',*opts,str(out/'watch.elf')],capture_output=True,timeout=180).returncode)
 if codes!=[0,5,5,5]:raise RuntimeError('PC watch probes failed')
 run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-control-input.ld','-o',out/'probe.elf',out/'probe.o'])
 guards=[x for pc in WATCHES for x in ['--watch-pc-int',pc]]
 with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--watch-cycles-int','900000000',*guards,out/'probe.elf'],stdout=stdout,stderr=stderr)
 output=(out/'output.bin').read_bytes();coverage=observations(data,output)
 if output[:512]!=output[-512:] or output[:512]!=image[0x8c21b0:0x8c23b0]:raise RuntimeError('Note table changed')
 artifacts={}
 for name,path in [('voice246-note-dispatch-input.bin',out/'inputs.bin'),('voice246-note-dispatch-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 sources=[Path(__file__).resolve(),templates/'voice-note-dispatch.s.in',ROOT/'Source/Voice246NoteDispatch.h',ROOT/'Tests/Voice246NoteDispatchTests.cpp',ROOT/'scripts/verify-drwave246-held-note-modes.py',ROOT/'scripts/verify-drwave246-poly-first-free.py',ROOT/'scripts/verify-drwave246-adjusted-note-chain.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'scripts/verify-voice246-release-selectors.py',templates/'voice-control-input.ld',templates/'parallel-halfword.s']
 sources+=list((ROOT/'Source').glob('*246*.h'))
 report={'ldr_sha256':expected,'entry':'0x01a85750','end_exclusive':'0x01a85840','code_bytes':240,'routine_sha256':hashlib.sha256(image[0xa85750:0xa85840]).hexdigest(),'inner_entry':'0x01a84604','records':CASES,'independent':INDEPENDENT,'streams':STREAMS,'calls_per_stream':CALLS,'coverage':coverage,'image_bytes':IMAGE,'input_record_bytes':RECORD,'result_record_bytes':RESULT,'regions':[{'address':hex(addr),'bytes':n,'retained_in_stream':retain} for addr,n,retain in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patches':{str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']},'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in sources},'pc_watches':WATCHES,'model':'GNU BF524 with both documented writeback patches; no hardware/timing claim','limits':['Discriminator zero, metadata zero, pitch zero, valid notes 0-127, selectors 1-3; no original note-range asserts, drum or notification path.','Argument raw and R1 explicit, not constructor/MIDI dispatcher cadence. Activity and gate supplied per call; ready ADSR, null auxiliaries and tuning explicit.','No render, pending-field consumer, buffer producer, clock or plugin integration. Selected shared RAM and local copy monitored, not full machine/pipeline.']}
 report['local_frame_address']='0x02ffffe8';report['frame_probe']='own LINK 0x10, SP 0x03000000';report['pc_watch_probe_exit_codes']=codes;report['pc_watch_original_run_exit_code']=0
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS original note dispatcher:',CASES,'records;',coverage,'full image',IMAGE)
if __name__=='__main__':main()
