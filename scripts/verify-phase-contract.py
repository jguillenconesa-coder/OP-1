#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce rev246 control-range/frequency probes and 88.2 kHz Pd oracles.
Original code is read only from an operator-supplied LDR and retained in a new
private directory outside the repository. No boot, flashing or hardware I/O.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(args,**kwargs):return subprocess.run([str(x) for x in args],check=True,timeout=60,**kwargs)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+flag,required=True,type=Path)
    a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a new output directory outside the repository')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Address profile only supports the inventoried rev246 LDR')
    spec=importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py');mapper=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
    raw=a.ldr.read_bytes();metadata,unused=mapper.inspect(raw);base=int(metadata['sdram_start'],16)
    image=bytearray(int(metadata['sdram_end'],16)-base);offset=0
    # Explicit fill words are needed by this probe. Ignore boot-only payloads.
    while offset<len(raw):
        flags,address,count,argument=struct.unpack_from('<4I',raw,offset)
        if base<=address and address+count<=base+len(image) and not flags&0x1000:
            payload=(struct.pack('<I',argument)*((count+3)//4))[:count] if flags&0x100 else raw[offset+16:offset+16+count]
            image[address-base:address-base+count]=payload
        offset+=16+(0 if flags&0x100 else count)
    out.mkdir(parents=True);(out/'sdram.bin').write_bytes(image)
    templates=ROOT/'research/blackfin';(out/'voice.s').write_text((templates/'voice-contract.s.in').read_text().replace('@WORK@',out.as_posix()))
    run([a.assembler,'-I',a.testutils,'-o',out/'voice.o',out/'voice.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'voice.elf',out/'voice.o'])
    simulator=[a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','300000000']
    with (out/'voice-contract.bin').open('wb') as stdout,(out/'voice.log').open('wb') as stderr:run(simulator+[out/'voice.elf'],stdout=stdout,stderr=stderr)
    contract=(out/'voice-contract.bin').read_bytes()
    if len(contract)!=988:raise RuntimeError('Incomplete original function probe')
    frequencies=[55,110,220,261.625565,440,880,1760];f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
    calculated=[int(f32(f32(88200)/f32(f32(x)*16384))*2**31) for x in frequencies]
    observed=list(struct.unpack_from('<7I',contract))
    if calculated!=observed:raise RuntimeError('Original frequency setter differs from the independently calculated contract')
    ranges=[list(struct.unpack_from('<4h',contract,28+(7*8+i)*8)) for i in range(4)]
    if ranges!=[[0,32767,32767,0],[0,29491,32767,0],[0,32767,32767,0],[0,32767,32767,0]]:raise RuntimeError('Phase control ranges changed')
    for name in ['pd-stream-probe','pd-full-probe']:(out/name).mkdir()
    (out/'pd-full-probe/kernel.bin').write_bytes(image[0x1a0bc5c-base:0x1a0beaa-base])
    sine=[round(math.sin(i*math.pi/4096)*32767) for i in range(8193)]
    (out/'pd-full-probe/table.bin').write_bytes(struct.pack('<8193h',*sine))
    stream=bytearray()
    for patch in json.loads((templates/'phase-controls.json').read_text())['presets']:
        for trajectory,note in enumerate((60,48,72)):
            for block in range(16):
                increments=[]
                for frame in range(128):
                    pos=block*128+frame;semitones=0 if trajectory==0 else 12*pos/2047 if trajectory==1 else math.sin(2*math.pi*8*pos/88200)
                    increments.append(round(440*2**((note-69+semitones)/12)/88200*2**32))
                stream+=struct.pack('<4h',*patch['controls'])+bytes(124)+struct.pack('<128II',*increments,int(block==0))
    probe=out/'pd-stream-probe';(probe/'inputs.bin').write_bytes(stream)
    (probe/'probe.s').write_text((templates/'pd-stream.s.in').read_text().replace('@WORK@',out.as_posix()).replace('626688','208896'))
    run([a.assembler,'-I',a.testutils,'-o',probe/'probe.o',probe/'probe.s']);run([a.linker,'-T',templates/'pd.ld','-o',probe/'probe.elf',probe/'probe.o'])
    with (probe/'results.bin').open('wb') as stdout,(probe/'sim.log').open('wb') as stderr:run(simulator+[probe/'probe.elf'],stdout=stdout,stderr=stderr)
    artifacts={}
    for name,path in [('voice-contract.bin',out/'voice-contract.bin'),('pd-clock-input.bin',probe/'inputs.bin'),('pd-clock-output.bin',probe/'results.bin')]:
        if path.read_bytes()!=(ROOT/'Tests/ReferenceVectors'/name).read_bytes():raise RuntimeError('Committed oracle mismatch: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path),'matches_committed_fixture':True}
    result={'ldr_sha256':expected,'model':'GNU BF524 simulator; isolated calls, no original boot/hardware validation','frequency_setter':'0x01a51a08','range_initializer':'0x01a1e914','range_table':'0x01de5890','phase_engine_type':7,'frequencies_hz':frequencies,'sampler_fixed_point':observed,'sampler_formula':'trunc(float32(88200 / float32(float32(hz) * 16384)) * 2^31)','phase_ranges_min_max_scale_kind':ranges,'pd_clock_hz':88200,'pd_samples_compared':98304,'pd_block_states_compared':768,'clock_scope':'88200 literal verified in sampler setter; common voice clock additionally supported by external op1-sdk analysis, not full original note preparation','artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]}}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: original ranges and seven frequency conversions; 98304 Pd samples / 768 block states at 88200 Hz; three oracles reproduced')
if __name__=='__main__':main()
