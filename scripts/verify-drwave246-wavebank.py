#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce the original Dr. Wave startup sequence and six generated banks.

Shared initialization clears the scratch wave before every bank build. Gains
other than the captured defaults are explicit test injections, not presets.
Original SDRAM, instructions and ELF stay in a new private output directory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BASE=0x01cc1864
END=0x01ceb8b0
REGION=END-BASE
RESULT=16+REGION+4
GAINS=[None]*6+[0,1,32767,32768,32769,65535,65536,-1,-32768,2147483647,-2147483648,123456789]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    return b''.join(struct.pack('<4I',i%6,int(g is not None),(g or 0)&0xffffffff,(0x31af45db+i*0x1234567)&0xffffffff) for i,g in enumerate(GAINS))
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--record',action='store_true');args=parser.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py')
    mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=args.ldr.read_bytes();meta,_=mapper.inspect(raw);base=int(meta['sdram_start'],16)
    image=bytearray(int(meta['sdram_end'],16)-base);cursor=0
    while cursor<len(raw):
        flags,address,n,argument=struct.unpack_from('<4I',raw,cursor)
        if base<=address and address+n<=base+len(image) and not flags&0x1000:
            image[address-base:address-base+n]=(struct.pack('<I',argument)*((n+3)//4))[:n] if flags&0x100 else raw[cursor+16:cursor+16+n]
        cursor+=16+(0 if flags&0x100 else n)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(inputs())
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'drwave-wavebank.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@WORDS@',str(REGION//4)).replace('@BYTES@',str(len(GAINS)*RESULT)))
    def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,timeout=180,**kwargs)
    run([args.assembler,'-I',args.testutils,'-o',out/'probe.o',out/'probe.s'])
    run([args.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([args.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=len(GAINS)*RESULT:raise RuntimeError('Incomplete original startup output')
    artifacts={}
    for name,path in [('drwave246-wavebank-input.bin',out/'inputs.bin'),('drwave246-wavebank-output.bin',out/'output.bin')]:
        fixture=ROOT/'Tests/ReferenceVectors'/name
        if args.record:fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes()!=path.read_bytes():raise RuntimeError('Original startup oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    banks=[{'address':hex(a),'waves':n,'harmonics':h,'direction':direction} for a,n,h,direction in
           [(0x01cc208c,64,'all','descending'),(0x01cca08c,64,'all','ascending'),
            (0x01cd208c,64,'odd','descending'),(0x01cda08c,64,'odd','ascending'),
            (0x01ce208c,32,'alternating odd / square','descending'),(0x01ce608c,32,'alternating odd / square','ascending')]]
    report={'ldr_sha256':expected,'shared_initializer':'0x0197d774','bank_initializer':'0x0197cdec',
            'sine_source':'0x018b6688','sine_runtime':'0x0101e5e8','sine_contract':'8192 low signed16 words from the previously verified source; explicit harness copy, not the full original sine initializer.',
            'settings':'0x01cc1864','runtime_waves':'0x01cea08c','shared_increments':'0x01ceb88c',
            'captured_default_reference_gain':32768,'captured_default_output_gain':262144,
            'clean_scratch_sequence':True,'cases':len(GAINS),'default_cases':6,'explicit_gain_cases':len(GAINS)-6,
            'waves_per_build':320,'samples_per_wave':256,'table_samples_compared':len(GAINS)*81920,
            'captured_region':{'start':hex(BASE),'end_exclusive':hex(END),'bytes':REGION},'result_bytes_per_case':RESULT,
            'banks':banks,'artifacts':artifacts,'model':'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
            'tools':{str(x):sha(x) for x in [args.assembler,args.linker,args.simulator]},
            'limitations':['Startup with a clean scratch wave only. Dirty-scratch reinitialization is not reconstructed.',
                           'Only table generation and shared initialization are checked. The scheduled morph/resample/update route, complete voice, scheduler, ADSR, output and plugin integration remain pending.',
                           'Non-default reference gains are explicit stress inputs, not recovered settings or presets.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {len(GAINS)} original startup calls, {len(GAINS)*81920} generated table samples, six banks / 320 waves per build')
if __name__=='__main__':main()
