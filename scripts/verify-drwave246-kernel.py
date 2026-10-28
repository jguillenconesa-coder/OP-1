#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Execute the entire original Dr. Wave render with explicit external events.
Generated base banks, spectral update, transforms and oscillator output are
contrasted together. Original firmware/code/ELF remain private.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES=512
RECORD=9316
RESULT=8784
BANK=163840
SEEDS=(32768,29127,26214,23831,21845,20164,18724,17476)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    seed=0x44524b4e
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    blue=[0,1,2730,2731,5461,8191,8192,8193,10922,13653,16383,16384,16385,19114,21845,24575]
    green=[0,1,4096,8191,8192,8193,12288,16383,16384,24575,32767]
    white=[0,1,4096,8191,8192,8193,12288,16383,16384,20480,24575,28672,32767]
    orange=[0,1,127,8192,16384,32766,32767]
    data=bytearray()
    for case in range(CASES):
        independent=case<256;block=(case-256)%64
        action=int(independent or block==0)|(2 if case%13==0 else 0)|(4 if case%29==0 else 0)|(8 if case%31==0 else 0)
        slot=case//2%6;counter_slot=slot if case%2==0 else (slot+1)%6
        counter=(rnd()&0xffff0000)|(counter_slot+6*(rnd()%5461))
        enabled=0 if case%17==0 else (1,128,255)[case%3]
        controls=[blue[case//2%len(blue)],green[case//32%len(green)],white[case//4%len(white)],orange[case//6%len(orange)]] if independent else [rnd()%24576,rnd()%32768,rnd()%32768,rnd()%32768]
        pitch=[rnd() for _ in range(128)]
        if independent:pitch[0]=(0,1,255,256,0x7fffffff,0x80000000,0xffffffff)[case//8%7]
        pool=[]
        for voice in range(6):
            inversion=(0,1,0x00ffffff,0x01000000,0x01ffffff)[(case+voice)%5] if independent else rnd()&0x1ffffff
            pool.extend([rnd(),rnd(),*[rnd()&0xffffff for _ in range(5)],voice,inversion])
        waves=[rnd() for _ in range(1536)]
        shared=bytearray(struct.pack('<522I',*[rnd() for _ in range(522)]))
        struct.pack_into('<4I',shared,0,rnd(),rnd(),32768,(0,65536,262144)[case%3])
        struct.pack_into('<2I',shared,16,(0,1,7,0xffffffff)[case//3%4],(0,1,7,0xffffffff)[case//9%4])
        flag=(0,1,7,0xffffffff)[case//5%4]
        if flag:struct.pack_into('<8H',shared,24,*SEEDS)
        data+=struct.pack('<4I4h128I54I1536I',action,slot,counter,enabled,*controls,*pitch,*pool,*waves)+shared+struct.pack('<5I8III68I',*[rnd() for _ in range(5)],*[rnd() for _ in range(8)],rnd(),flag,*[rnd() for _ in range(68)])
    if len(data)!=CASES*RECORD:raise RuntimeError('Invalid kernel input size')
    return data
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');args=p.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16);image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',arg)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin';(out/'probe.s').write_text((templates/'drwave-kernel.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(command,**kw):subprocess.run([str(x) for x in command],check=True,timeout=180,**kw)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s']);run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes();source=(out/'inputs.bin').read_bytes()
    if len(output)!=2*BANK+CASES*RESULT:raise RuntimeError('Incomplete original kernel output')
    if output[:BANK]!=output[-BANK:]:raise RuntimeError('Original kernel mutated base banks')
    artifacts={}
    for name,path in [('drwave246-kernel-input.bin',out/'inputs.bin'),('drwave246-kernel-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Kernel oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    counts={'updated':0,'read_only':0,'inactive':0}
    for case in range(CASES):
        _,slot,counter,enabled=struct.unpack_from('<4I',source,case*RECORD)
        counts['inactive' if not enabled else ('updated' if slot==(counter&65535)%6 else 'read_only')]+=1
    report={'ldr_sha256':expected,'original_render':'0x0197c870','original_wave_update':'0x0197c668','records':CASES,'counts':counts,'audio_samples':(CASES-counts['inactive'])*128,
            'independent_cases':256,'continuous_streams':4,'blocks_per_stream':64,'controls_domain':[[0,24575],[0,32767],[0,32767],[0,32767]],
            'phase_inversion_state_bits':25,'bank_initializer':'0x0197cdec','base_banks_unchanged':True,'base_bank_bytes':BANK,
            'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation','artifacts':artifacts,
            'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'limitations':['Complete isolated render, including scheduled wave update, for the tested nonnegative control domain. Wider green/white words are stress coverage, not a claim about physical knob ranges.',
                           'The external update counter, active flag, callbacks, pitch buffer and schedule are explicit inputs. No original allocator, pitch calibration/provider, ADSR, output conversion, physical timing or plugin integration claimed.',
                           'Sine source words are explicitly copied by the harness; full original sine initialization is not executed.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original complete Dr. Wave render recorded',counts)
if __name__=='__main__':main()
