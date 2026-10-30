#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast original workspace initialization, callback and both isolated renderers.
Both initial buffers, controls and event order remain explicit. This does not run
the partially encrypted original register-to-buffer handoff.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,BASE,EXTRA,RECORD,RESULT,BANK=1506,9316,1404,10720,10956,163840
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs(base):
    spec=importlib.util.spec_from_file_location('dispatch_input',ROOT/'scripts/verify-drwave246-control-dispatch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    old,original=m.inputs(base);data=bytearray();expanded=bytearray();seed=0x434f5245
    for case in range(CASES):
        row=bytearray(old[case*1176:(case+1)*1176]);image=[]
        for i in range(57):seed=(seed*1664525+1013904223)&0xffffffff;image.append(seed)
        row+=struct.pack('<57I',*image)
        data+=row;expanded+=original[case*10492:case*10492+BASE]+row
    return data,expanded
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    basepath=ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin';base=basepath.read_bytes()
    if len(base)!=512*BASE:raise ValueError('Wrong base fixture size')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    raw=a.ldr.read_bytes();meta,_=m.inspect(raw);start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700);changes,expanded=inputs(base)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-control-workspace.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
    run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
    with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--watch-cycles-int','9000000',out/'halfword.elf'],stdout=stdout,stderr=stderr)
    if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):
        raise RuntimeError('Simulator loses disjoint halfword writes; apply BOTH documented patches')
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'drwave-control-dispatch.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','128M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=112+BANK*2+CASES*RESULT or output[112:112+BANK]!=output[-BANK:]:raise RuntimeError('Incomplete output or changed base banks')
    counts={'applied':0,'inactive':0,'wave_update':0,'read_only':0,'render_inactive':0,'adsr_calls':0,'adsr_writes':0};matrices=[set(),set()]
    for case in range(CASES):
        x=changes[case*EXTRA:(case+1)*EXTRA];kind=struct.unpack_from('<I',x)[0];controls=struct.unpack_from('<8h',x,4)
        combination=0;power=1;applied=False
        for i in range(4):
            destination=controls[i+4]//2048;target=controls[i]//4096
            route=0 if destination==2 else 1 if destination==0 else 4 if destination==1 else 2+target
            combination+=route*power;power*=5
            applied |= (kind==0 and destination in (0,3)) or (kind==1 and destination==1)
        if kind<2:matrices[kind].add(combination)
        e=112+BANK+case*RESULT
        if struct.unpack_from('<I',output,e+8784)[0]!=int(applied):raise RuntimeError('Unexpected callback return')
        counts['applied' if applied else 'inactive']+=1
        p=expanded[case*RECORD:case*RECORD+BASE];_,slot,counter,enabled=struct.unpack_from('<4I',p)
        if kind==1:
            counts['adsr_calls']+=1
            counts['adsr_writes']+=1<=struct.unpack_from('<I',output,e+9844+slot*24)[0]<=8
        else:counts['render_inactive' if not enabled else 'wave_update' if slot==(counter&65535)%6 else 'read_only']+=1
    if any(len(m)!=625 for m in matrices):raise RuntimeError('Missing kind-0/1 routing combination')
    artifacts={}
    for name,path in [('drwave246-control-workspace-overrides.bin',out/'overrides.bin'),('drwave246-control-workspace-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'original_workspace_initializer':'0x01968774','initializer_calls':CASES,'original_decoder':'0x01a84f8c','decoder_calls':CASES*4,'original_callback':'0x019cce68','callback_end_exclusive':'0x019cd01c','original_drwave_render':'0x0197c870','original_adsr_render':'0x01912128','records':CASES,'independent_cases':1250,'continuous_streams':4,'calls_per_stream':64,'kind0_combinations':len(matrices[0]),'kind1_combinations':len(matrices[1]),'counts':counts,'drwave_active_audio_samples':(counts['wave_update']+counts['read_only'])*128,'adsr_sample_positions':counts['adsr_calls']*128,'adsr_generated_samples':counts['adsr_writes']*128,'pitch_words_compared':CASES*128,'amplitude_samples_compared':CASES*128,'base_banks_unchanged':True,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with both documented D-register and disjoint-halfword patches; not hardware validation','limitations':['Current/selected module types are explicitly 2, type flag is original zero and ADSR pool is explicitly initialized. Other types, lazy construction and assert paths excluded.','Original Core initializer clears the working image before callback each record; this invocation schedule is harness behavior, not an original caller. Renderer choice remains explicit.','Prior workspace bytes, both buffers, event order, counter and ADSR state/volume/seed are explicit. The original initializer supplies zero parameters, but the per-call producer, ADSR handoff, output mix and integration remain unknown.','Firmware, original instructions, disassembly and ELF remain private.']}
    report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
    report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'Source/Core246ControlWorkspace.h',ROOT/'Source/DrWave246ControlDispatch.h',ROOT/'Source/DrWave246MixedControl.h',ROOT/'Source/ControlEnvelope246Mode.h',ROOT/'Source/Envelope246.h',ROOT/'Tests/DrWave246ControlWorkspaceTests.cpp',ROOT/'scripts/verify-drwave246-control-dispatch.py',templates/'drwave-control-workspace.s.in',templates/'drwave-control-dispatch.ld',templates/'parallel-halfword.s',Path(__file__).resolve()]}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original Core initialization followed by type-2 callback/renderers',counts)
if __name__=='__main__':main()
