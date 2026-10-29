#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Run four-source ENV routing followed by ADSR privately; no original scheduler."""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,RECORD,RESULT=1024,1140,1280
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
    spec=importlib.util.spec_from_file_location('base_env',ROOT/'scripts/verify-control-envelope246.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
    initial=base.inputs();data=bytearray();seed=0x4d4f4445
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    for case in range(CASES):
        row=bytearray(RECORD);row[:440]=initial[case*440:(case+1)*440]
        # First half exercises mode only; second half mixes all five ENV colours.
        for i in range(4):
            target=4 if case<512 else (case//16+i)%5
            struct.pack_into('<h',row,72+i*2,target*4096+rnd()%4096)
        struct.pack_into('<h',row,56,rnd()%32768)
        struct.pack_into('<I15hHI',row,408,rnd(),64,16320,16384,64,16320,16384,0,32767,16384,64,16320,16384,2048,14336,32767,0,rnd())
        struct.pack_into('<I',row,448,rnd())
        for voice in range(6):
            struct.pack_into('<6I',row,452+voice*24,(case+voice)%9,rnd(),rnd()%1073741825,rnd(),rnd(),(rnd()&0xffff0000)|(rnd()%32768))
        struct.pack_into('<6I',row,596,rnd(),case%2,2,2,1,rnd())
        struct.pack_into('<I',row,620,case%6)
        struct.pack_into('<129I',row,624,*[rnd() for _ in range(129)])
        data+=row
    return data
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Unsupported LDR')
    spec=importlib.util.spec_from_file_location('mapper',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    raw=a.ldr.read_bytes();meta,_=m.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True,mode=0o700);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'control-envelope246-mode.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size!=32+CASES*RESULT:raise RuntimeError('Incomplete output')
    artifacts={}
    for name,source in [('control-envelope246-mode-input.bin','inputs.bin'),('control-envelope246-mode-output.bin','output.bin')]:
        path=out/source;target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'callback':'0x019cce68..0x019cd01c','range_initializer':'0x01a1e914','range_helper':'0x019f69b0','adsr_render':'0x01912128','records':CASES,'samples':CASES*128,'independent':512,'streams':8,'calls_per_stream':64,'artifacts':artifacts,'tools':{str(t):sha(t) for t in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with deferred D-register patch; not hardware validation','limitations':['ENV colours 0..4 and disabled destinations, current module type explicitly 2 (Dr. Wave). Type 4 module callbacks excluded. Initialized ADSR pool explicitly supplied.','Initial parameters, source values, event order, state and dither seed are explicit; not original ADSR handoff, pitch producer, cadence or plugin integration.','Original common range initializer is run once and its five descriptors captured; per-record range images are explicit copies with varied guards.','Original instructions, firmware and ELF remain private.']}
    report['coverage_verified_by_cpp_comparator']={'routing_masks':16,'colours':5,'adsr_stages':9,'applied':872,'inactive':152,'shared_dither_words':131072,'module_type':2,'pool_initialized':True}
    report['source_sha256']={str(path.relative_to(ROOT)):sha(path) for path in [ROOT/'Source/ControlEnvelope246Mode.h',ROOT/'Tests/ControlEnvelope246ModeTests.cpp',Path(__file__).resolve(),templates/'control-envelope246-mode.s.in']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: bounded Dr. Wave mode callback and 131072 ADSR samples recorded/reproduced')
if __name__=='__main__':main()
