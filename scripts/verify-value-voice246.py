#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Reproduce isolated rev246 Value voice/ENV routing with operator-supplied firmware.
Original instructions/ELF remain outside the repository in a new private directory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    data = bytearray(); seed = 0x564f4943
    def rnd():
        nonlocal seed
        seed = (seed * 1664525 + 1013904223) & 0xffffffff
        return seed
    for i in range(4096):
        state = struct.pack('<3I', *[rnd() for _ in range(3)])
        display = struct.pack('<8I', *[rnd() for _ in range(8)])
        speed = rnd()%16385
        if i < 128: speed = (0,1023,1024,2048,4096,7168,8191,8192,8193,8256,9000,12288,16383,16384)[i%14]
        amount = int(rnd()%65535)-32767
        if i < 128: amount = (-32767,0,1,32767)[i//14%4]
        destination = i//2%2
        kind = destination
        if i < 2048 and i%13 == 0: kind = (destination+1)%3
        controls = [speed,amount,destination*4096+(2048 if i%2 else 0),i//4%4*4096+rnd()%4096,
                    *[int(rnd()%65536)-32768 for _ in range(4)]]
        targets = [int(rnd()%65536)-32768 for _ in range(8)]
        bpm = (40.,60.,90.,120.,127.5,180.,220.,300.)[i%8]
        tempo = rnd() if i < 2048 else int(struct.unpack('<f',struct.pack('<f',1662149.125*bpm))[0])
        action = (2 if i%3 == 0 else 1) if i < 2048 else (2 if i%128 == 0 else 0)
        data += state + display + struct.pack('<8h8h4If',*controls,*targets,tempo,kind,(1,7)[i//16%2],action,bpm)
    return data

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['ldr','assembler','linker','simulator','testutils','output']: p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--record',action='store_true')
    a = p.parse_args(); out = a.output.resolve(); templates = ROOT/'research/blackfin'
    if out.exists() or out.is_relative_to(ROOT): raise ValueError('Choose a NEW private directory outside the repository')
    expected = json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr) != expected: raise ValueError('Only the inventoried rev246 LDR is supported')
    spec = importlib.util.spec_from_file_location('ldr_map',ROOT/'scripts/map-op1-ldr.py'); mapper = importlib.util.module_from_spec(spec); spec.loader.exec_module(mapper)
    raw = a.ldr.read_bytes(); meta,_ = mapper.inspect(raw); base = int(meta['sdram_start'],16); image = bytearray(int(meta['sdram_end'],16)-base); offset = 0
    while offset < len(raw):
        flags,address,count,argument = struct.unpack_from('<4I',raw,offset)
        if base <= address and address+count <= base+len(image) and not flags&0x1000:
            image[address-base:address-base+count] = (struct.pack('<I',argument)*((count+3)//4))[:count] if flags&0x100 else raw[offset+16:offset+16+count]
        offset += 16+(0 if flags&0x100 else count)
    out.mkdir(parents=True); (out/'sdram.bin').write_bytes(image); data = inputs(); blocks = len(data)//96
    (out/'inputs.bin').write_bytes(data)
    source = (templates/'value-voice-stream.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(blocks*68))
    (out/'probe.s').write_text(source)
    def run(args,**kw): subprocess.run([str(x) for x in args],check=True,timeout=60,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']); run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','300000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    if (out/'output.bin').stat().st_size != 152+blocks*68: raise RuntimeError('Incomplete Value voice/ENV output')
    artifacts = {}
    for name,path in [('value-voice246-input.bin',out/'inputs.bin'),('value-voice246-output.bin',out/'output.bin')]:
        fixture = ROOT/'Tests/ReferenceVectors'/name
        if a.record: fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes() != path.read_bytes(): raise RuntimeError('Oracle differs: '+name)
        artifacts[name] = {'bytes':path.stat().st_size,'sha256':sha(path),'matches_committed_fixture':True}
    evidence = {'ldr_sha256':expected,'model':'GNU 13.1 BF524 with documented local D-register writeback patch; not hardware validation',
        'process':'0x019cfaf8 (synth instrument/first four ENV parameters only)','note_reset':'0x019cfd5a','range_helper':'0x01a1dfec / 0x019f69b0','range_initializer':'0x01a1e914',
        'instrument_types':[1,7],'digital_ranges':'0x01de58d0','phase_ranges':'0x01de5a50','envelope_ranges':'0x01de5710','tempo_float_multiply':'0x01aa55a0','tempo_float_to_integer':'0x01aa4dbc',
        'tempo_constant_float32':1662149.125,'tempo_setter_calls':'0x01a5e850 / 0x01a5e908 / 0x01a5e972',
        'independent_cases':2048,'stream_cases':2048,'cases':blocks,'full_self_bytes':12,'full_display_bytes':32,'target_words':8,
        'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'patch_sha256':sha(templates/'gnu-13.1-dreg-writeback.patch'),
        'limitations':['Instrument kinds 0 and ENV kind 1, native Digital/Phase, first four coloured parameters only. Other types, auxiliary controls, play modes and drum destinations are not reconstructed.',
            'The integrated 64-sample / 44.1 kHz control clock is an assumption; isolated results do not establish complete instrument timing or acoustic fidelity.']}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'PASS: {blocks} Value voice/ENV complete states / displays / target buffers / return codes and tempo calculations reproduced')
if __name__ == '__main__': main()
