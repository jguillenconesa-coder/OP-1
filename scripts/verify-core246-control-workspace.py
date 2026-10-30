#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast the original Core workspace initializer and its complete bounded footprint.
Prior workspace images and byte pokes are explicit. This does not run
the partially encrypted original register-to-buffer handoff.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,RESULT=4096+256,424
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
    data=bytearray();seed=0x574f524b
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff;return seed
    for case in range(CASES):
        image=bytearray(struct.pack('<106I',*[rnd() for _ in range(106)]))
        if case<3:image[:228]=bytes([[0,0xa5,0xff][case]])*228
        data+=struct.pack('<3I',int(case<=4096),case%228,rnd()%256)+image
    return data,data
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    raw=a.ldr.read_bytes();meta,_=m.inspect(raw);start=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-start);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if start<=address and address+n<=start+len(image) and not flags&0x1000:
            image[address-start:address-start+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700);changes,expanded=inputs()
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'core-control-workspace.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=CASES*RESULT:raise RuntimeError('Incomplete output')
    artifacts={}
    for name,path in [('core246-control-workspace-input.bin',out/'overrides.bin'),('core246-control-workspace-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'routine':'0x01968774','routine_end_exclusive':'0x019687c0','zero_callback':'0x01968768','array_constructor':'0x01aa3862','records':CASES,'independent_cases':4096,'continuous_calls':256,'workspace_image_bytes':228,'written_bytes':82,'flag_word':'0xff8016dc','common_workspace':'0xff801724','common_words':8,'secondary_workspace':'0xff801774','secondary_words':32,'pitch_image_bytes':40,'adsr_pool_image_bytes':148,'rng_image_bytes':8,'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'limitations':['Complete isolated initializer, not original boot or the per-call parameter producer.','The 256-call stream has explicit byte pokes between calls; not an original reset schedule.','Workspace role beyond the common eight-word image is not inferred. No complete voice/pitch/ADSR handoff or integration.','Firmware, instructions, disassembly and ELF remain private.']}
    report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'Source/Core246ControlWorkspace.h',ROOT/'Tests/Core246ControlWorkspaceTests.cpp',templates/'core-control-workspace.s.in',Path(__file__).resolve()]}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: 4096 workspace initializations + 256 continuous calls; full image, neighbors, pitch/ADSR/RNG preserved')
if __name__=='__main__':main()
