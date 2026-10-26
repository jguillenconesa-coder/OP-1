#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated rev246 ADSR oracles, stock-model hazards and deferred writes.
Original firmware/code stays in a NEW operator directory outside the repository.
Requires two GNU 13.1 tools: original and separately D-register-patched simulator.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(args,**kw):return subprocess.run([str(x) for x in args],check=True,timeout=60,**kw)
def random_inputs():
    data=bytearray();state=0x41445352
    def rnd():
        nonlocal state
        state=(state*1664525+1013904223)&0xffffffff
        return state
    for i in range(4096):
        rnd();stage=i%9
        controls=[(0,64,127,128,4096,8192,16256,16383)[(i//(8**j))%8] for j in range(4)]
        controls[2]=(0,1,8192,16384,24576,32767)[i%6]
        level=[0,1,16,65536,1073741824,state%1073741825][i%6]
        gain=volume=32767;previous=0
        if i>=512:
            controls=[rnd()%16384 for _ in range(4)];controls[2]=rnd()%32768
            gain=rnd()%32768;volume=rnd()%32768;previous=int(rnd()%65536)-32768
        data+=struct.pack('<6I4hHHIII',stage,i,level,0,0,gain,*controls,volume,0,state,previous&0xffffffff,0)
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ['ldr','assembler','linker','simulator','stock-simulator','testutils','output']:p.add_argument('--'+flag,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();templates=ROOT/'research/blackfin'
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a new private directory outside this repository')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if digest(a.ldr)!=expected:raise ValueError('Only the inventoried rev246 LDR is supported')
    spec=importlib.util.spec_from_file_location('map_ldr',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();meta,unused=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);offset=0
    while offset<len(raw):
        flags,address,count,argument=struct.unpack_from('<4I',raw,offset)
        if base<=address and address+count<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+count]=(struct.pack('<I',argument)*((count+3)//4))[:count] if flags&0x100 else raw[offset+16:offset+16+count]
        offset+=16+(0 if flags&0x100 else count)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image)
    def build(name,source,linker='voice-contract.ld'):
        path=out/(name+'.s');path.write_text(source)
        run([a.assembler,'-I',a.testutils,'-o',out/(name+'.o'),path]);run([a.linker,'-T',templates/linker,'-o',out/(name+'.elf'),out/(name+'.o')])
        return out/(name+'.elf')
    def simulate(tool,elf,path,regions=True):
        args=[tool,'--model','bf524','--memory-size','64M','--watch-cycles-int','300000000']
        if regions:args+=['--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000']
        with path.open('wb') as stdout,path.with_suffix('.log').open('wb') as stderr:run(args+[elf],stdout=stdout,stderr=stderr)
    micro=build('micro',(templates/'parallel-dreg.s').read_text(),'flat.ld')
    expected_words=(0x11112222,7,0x33334444,3,0x55556666,0x77778888,0x1234,0x43214321,0x13572468,0);micro_results={}
    for model,tool in [('stock',a.stock_simulator),('dreg_deferred',a.simulator)]:
        path=out/('micro-'+model+'.bin');simulate(tool,micro,path,False);words=struct.unpack('<10I',path.read_bytes());micro_results[model]=[hex(x) for x in words]
        if (words==expected_words)!=(model=='dreg_deferred'):raise RuntimeError('Simulator does not satisfy the expected packet microprobes: '+model)
    # Original helper transforms the original float source tables to Q31; own
    # scalar load/call/store harness avoids parallel-copy hazards during setup.
    prefix='.include "testutils.inc"\nstart\nimm32 SP, 0x03000000\n'
    for i in range(256):prefix+=f'imm32 P0, {0x18c13c4+i*4}\nR0 = [P0];\nimm32 P0, 0x01aa6fd0\nCALL (P0);\nimm32 P0, {0x106b3cc+i*4}\n[P0] = R0;\n'
    (out/'inputs.bin').write_bytes(random_inputs())
    stream=bytearray()
    for preset,patch in enumerate(json.loads((templates/'phase-envelopes.json').read_text())['presets']):
        for block in range(1024):
            action=1 if block in (0,896) else 2 if block in (384,960) else 3 if block==768 else 0
            stream+=struct.pack('<6I4hHHIII',0,0,0,0,0,32767,*patch['adsr'][:4],32767,0,0x24600000+preset,0,action)
    (out/'stream-inputs.bin').write_bytes(stream)
    random_elf=build('random',prefix+(templates/'adsr-random.s.in').read_text().replace('@WORK@',out.as_posix()))
    stream_elf=build('stream',prefix+(templates/'adsr-stream.s.in').read_text().replace('@WORK@',out.as_posix()))
    simulate(a.stock_simulator,random_elf,out/'random-stock.bin');simulate(a.simulator,random_elf,out/'random.bin');simulate(a.simulator,stream_elf,out/'stream.bin')
    artifacts={}
    for name,path in [('adsr246-input.bin',out/'inputs.bin'),('adsr246-output.bin',out/'random.bin'),('adsr246-stock-output.bin',out/'random-stock.bin'),('adsr246-stream-input.bin',out/'stream-inputs.bin'),('adsr246-stream-output.bin',out/'stream.bin')]:
        if path.read_bytes()!=(ROOT/'Tests/ReferenceVectors'/name).read_bytes():raise RuntimeError('Committed oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':digest(path),'matches_committed_fixture':True}
    result={'ldr_sha256':expected,'model':'GNU 13.1 BF524 with local D-register deferred-write patch; original instructions unchanged; not hardware validation','stock_model_preserved':True,'source_coefficients':['0x018c13c4','0x018c15c4'],'coefficient_conversion':'0x01aa6fd0','render':'0x01912128','note_on':'0x0191211a','release':'0x019120d0','fast_stop':'0x0191209c','random_blocks':4096,'random_samples':524288,'stream_blocks':16384,'stream_samples':2097152,'preset_envelopes':16,'microprobes':{'expected':[hex(x) for x in expected_words],'models':micro_results},'artifacts':artifacts,'tools':{str(x):digest(x) for x in [a.assembler,a.linker,a.stock_simulator,a.simulator]},'patch_sha256':digest(templates/'gnu-13.1-dreg-writeback.patch'),'limitations':['Only the documented D-register packet writeback hazards are corrected, not a general validation of the complete simulator.','Original boot, full preset control/LFO/velocity path, output mix, DSP timing and hardware audio are not validated.','Production adds its own NCO/filter/rate adapter and envelope normalization; dither state is per voice, not the original shared global stream.']}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: five packet microprobes; five oracle files reproduced; 2,621,440 original ADSR sample outputs and 20,480 complete block states')
if __name__=='__main__':main()
