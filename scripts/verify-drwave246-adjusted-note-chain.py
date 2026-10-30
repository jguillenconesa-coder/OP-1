#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Original adjustment setters followed by constructor, events and explicit ADSR render.
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
 (0x01aaac0c,8,False),(0x01aaad14,32,False),(0x027fd9dc,12,False),(0x018c2180,12,False),(0x01de4c3c,12,False),(0x01dfe4d8,48,True)]
IMAGE=sum(n for _,n,_ in REGIONS);RECORD=40+IMAGE+272;RESULT=IMAGE*2+364
OFF=[]
for _,n,_ in REGIONS:OFF.append(sum(x[1] for x in REGIONS[:len(OFF)]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
 spec=importlib.util.spec_from_file_location('events',ROOT/'scripts/verify-drwave246-note-event-chain.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 old=m.inputs();data=bytearray();table=None
 for c in range(CASES):
  row=old[c*m.RECORD:(c+1)*m.RECORD];header=bytearray(row[:32]);image=bytearray(row[32:32+IMAGE]);reset=struct.unpack_from('<I',header)[0]
  if reset:table=[(c+i)%9-4 for i in range(8)]
  for i,v in enumerate(table):struct.pack_into('<i',image,OFF[14]+8+i*4,v)
  slot=(c//8)%8;value=(c+c//64)%9-4;table[slot]=value
  selected=struct.unpack_from('<I',image,OFF[8])[0];discriminator=struct.unpack_from('<I',image,OFF[13]+4)[0]
  received=(c%128-53-(0 if discriminator==1 else table[selected]*12))&0xffffffff
  struct.pack_into('<I',header,16,received)
  data+=header+struct.pack('<2i',slot,value)+image+row[-272:]
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
 source=(templates/'drwave-adjusted-note-chain.s.in').read_text()
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
 counts={'activations':0,'accepted':0,'blocked':0,'releases':0,'constructor_bypass':0,'constructor_table':0,'written_samples':0,'preserved_samples':0,'initializing_setters':0,'changing_setters':CASES,'lookup_updated_slot':0,'lookup_other_slot':0,'identical_setter_values':0}
 all_output=(out/'output.bin').read_bytes()
 if all_output[:512]!=all_output[-512:] or all_output[:512]!=image[0x018c21b0-base:0x018c23b0-base]:raise RuntimeError('Note table changed')
 oracle=all_output[512:-512];input_bytes=(out/'inputs.bin').read_bytes()
 for c in range(CASES):
  x=input_bytes[c*RECORD:(c+1)*RECORD];e=oracle[c*RESULT:(c+1)*RESULT]
  discriminator=struct.unpack_from('<I',x,40+OFF[13]+4)[0]
  counts['constructor_bypass' if discriminator==1 else 'constructor_table']+=1
  reset=struct.unpack_from('<I',x)[0];slot,value=struct.unpack_from('<2i',x,32);counts['initializing_setters']+=8*bool(reset)
  selected=struct.unpack_from('<I',x,40+OFF[8])[0]
  if discriminator!=1:counts['lookup_updated_slot' if selected==slot else 'lookup_other_slot']+=1
  counts['identical_setter_values']+=struct.unpack_from('<i',x,40+OFF[14]+8+slot*4)[0]==value
  if struct.unpack_from('<I',x,12)[0]:counts['releases']+=1
  else:
   counts['activations']+=1
   counts['accepted' if struct.unpack_from('<I',x,40+OFF[8])[0]==struct.unpack_from('<I',x,40+OFF[9])[0] else 'blocked']+=1
  v=struct.unpack_from('<I',x,4)[0];stage=struct.unpack_from('<I',e,2580+v*24)[0]
  counts['written_samples' if 1<=stage<=8 else 'preserved_samples']+=128
 artifacts={}
 for name,path in [('drwave246-adjusted-note-chain-input.bin',out/'inputs.bin'),('drwave246-adjusted-note-chain-output.bin',out/'output.bin')]:
  fixture=ROOT/'Tests/ReferenceVectors'/name
  if a.record:fixture.write_bytes(path.read_bytes())
  if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
  artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
 report={'ldr_sha256':expected,'setter':'0x01a8633c','constructor':'0x01a85014','activation':'0x01a84414','release':'0x01a85058','adsr_render':'0x01912128','selectors':[0,1,2,3,4],'records':CASES,'independent':512,'streams':4,'blocks_per_stream':64,'sample_positions':CASES*128,'counts':counts,'image_bytes':IMAGE,'input_bytes':RECORD,'output_bytes':RESULT,'regions':[{'address':hex(a),'bytes':n,'retained_in_stream':r} for a,n,r in REGIONS],'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Composition order and action/voice selection are harness inputs, not the recovered original dispatcher or allocator. Lists seeded explicitly; activation does not insert into the list or trigger common ADSR in this routine.','Eight valid initial values and one setter change per call are explicit. Original setters supply the adjustment table after a poison prefill; no direct table override follows. Constructed notes are 0..127. Tuning, selection, controls and ordering remain explicit.','No Dr. Wave audio render, pitch/volume-buffer producer, pending-field consumer, clock, mixer, output or host integration is demonstrated.','ADSR ready flag set and valid Core pointers/Dr. Wave vtables; other Dr. Wave fields are preserved stress patterns, not claimed render-ready states. Nonnegative ADSR ranges; error paths excluded. Firmware/instructions/ELF stay private.']}
 report['setter_table_snapshot']={'address':'0x01dfe4d8','bytes':48,'stage':'After original setters and before constructor; table plus neighbors.'}
 report['table_supply']={'poison_before_initializing_setters':True,'initializing_setters_per_reset':8,'changing_setters_per_record':1,'direct_override_after_setters':False,'valid_slots':[0,7],'valid_adjustments':[-4,4],'schedule':'Explicit harness order, not recovered boot/settings/dispatcher cadence.'}
 report['note_table']={'source':'0x018c21b0','destination':'0x01dfe530','words':128,'copy_fragment_start':'0x01a86700','copy_fragment_end_exclusive':'0x01a8672e','copy_fragment_sha256':hashlib.sha256(image[0x01a86700-base:0x01a8672e-base]).hexdigest(),'scope':'Original copy fragment plus own RTS, not complete initializer; full table captured before/after. Leading list neighbor 0x01dfe72c is note word 127.'}
 report['tuning']={'index_address':'0x018c2184','explicit_index':1200,'coefficient_address':'0x027fd9e0','coefficient':'Explicit per-call input including zero, one, signs and extremes; no default calibration inferred.'}
 report['model']='GNU BF524 with both documented D-register/disjoint-halfword writeback patches; not hardware or pipeline validation'
 report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
 report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [Path(__file__).resolve(),templates/'drwave-adjusted-note-chain.s.in',ROOT/'Source/Envelope246.h',ROOT/'Source/Voice246PolyRelease.h',ROOT/'Source/Voice246ReleaseSelectors.h',ROOT/'Source/Voice246NoteArgument.h',ROOT/'Source/DrWave246Activation.h',ROOT/'Source/Voice246Activation.h',ROOT/'Tests/DrWave246AdjustedNoteChainTests.cpp',ROOT/'scripts/verify-voice246-release-selectors.py',ROOT/'scripts/verify-drwave246-note-event-chain.py',ROOT/'Source/Voice246NoteAdjustment.h',ROOT/'Source/Voice246NoteTable.h',ROOT/'Source/DrWave246Routing.h',ROOT/'Source/DrWave246.h',ROOT/'Source/ReferenceDsp.h',ROOT/'Source/Envelope246Tables.h',templates/'voice-control-input.ld',templates/'parallel-halfword.s']}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original adjusted note chain:',CASES,'records; image',IMAGE,'record',RECORD,'result',RESULT)
if __name__=='__main__':main()
