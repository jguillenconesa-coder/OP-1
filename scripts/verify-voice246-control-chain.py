#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast original control decoding followed by volume/pitch modification and Dr. Wave.
Both initial buffers, controls and event order remain explicit. This does not run
the partially encrypted original register-to-buffer handoff.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,BASE,EXTRA,RECORD,RESULT,BANK=512,9316,396,9712,9692,163840
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs(base):
    spec=importlib.util.spec_from_file_location('volume_input',ROOT/'scripts/verify-control-volume246.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    old,_=m.inputs(base);data=bytearray();expanded=bytearray()
    for case in range(CASES):
        row=bytearray(old[case*380:(case+1)*380])
        for i in range(4):row+=bytes([(case*13+i)&255,i+1,(case+37*i)&255,(case*29+i)&255])
        # Full words are poisoned before original byte-value decoder clears them.
        struct.pack_into('<4I',row,28,0xdeadbeef,0xa5a5a5a5,0x80000000,0xffffffff)
        data+=row;expanded+=base[case*BASE:(case+1)*BASE]+row
    return data,expanded
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['ldr','assembler','linker','simulator','testutils','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--record',action='store_true');a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(a.ldr)!=expected:raise ValueError('Only inventoried rev246 is supported')
    basepath=ROOT/'Tests/ReferenceVectors/drwave246-kernel-input.bin';base=basepath.read_bytes()
    if len(base)!=CASES*BASE:raise ValueError('Wrong base fixture size')
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
    (out/'probe.s').write_text((templates/'voice-control-chain.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
    run([a.assembler,'-I',a.testutils,'-o',out/'halfword.o',templates/'parallel-halfword.s'])
    run([a.linker,'-T',templates/'flat.ld','-o',out/'halfword.elf',out/'halfword.o'])
    with (out/'halfword.bin').open('wb') as stdout,(out/'halfword.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--watch-cycles-int','9000000',out/'halfword.elf'],stdout=stdout,stderr=stderr)
    if struct.unpack('<5I',(out/'halfword.bin').read_bytes())!=(0x1234,0x0200abcd,0x1234,0xabcd0300,0x2468abcd):
        raise RuntimeError('Simulator loses disjoint halfword writes; apply BOTH documented patches')
    run([a.assembler,'-I',a.testutils,'-o',out/'probe.o',out/'probe.s']);run([a.linker,'-T',templates/'voice-contract.ld','-o',out/'probe.elf',out/'probe.o'])
    with (out/'output.bin').open('wb') as stdout,(out/'probe.log').open('wb') as stderr:
        run([a.simulator,'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--memory-region','0xff900000,0x8000','--watch-cycles-int','900000000',out/'probe.elf'],stdout=stdout,stderr=stderr)
    output=(out/'output.bin').read_bytes()
    if len(output)!=BANK*2+CASES*RESULT or output[:BANK]!=output[-BANK:]:raise RuntimeError('Incomplete output or changed base banks')
    counts={'applied':0,'inactive':0,'wave_update':0,'read_only':0,'render_inactive':0};masks=set();active_counts=set()
    for case in range(CASES):
        kind=struct.unpack_from('<I',changes,case*EXTRA)[0]
        controls=struct.unpack_from('<8h',changes,case*EXTRA+4);mask=sum(int(controls[i+4]//2048==3)<<i for i in range(4))
        masks.add(mask);active_counts.add(mask.bit_count());applied=bool(mask and kind==0)
        if struct.unpack_from('<I',output,BANK+case*RESULT+8784)[0]!=int(applied):raise RuntimeError('Unexpected callback return')
        counts['applied' if applied else 'inactive']+=1
        _,slot,counter,enabled=struct.unpack_from('<4I',base,case*BASE)
        counts['render_inactive' if not enabled else 'wave_update' if slot==(counter&65535)%6 else 'read_only']+=1
    artifacts={}
    for name,path in [('voice246-control-chain-overrides.bin',out/'overrides.bin'),('voice246-control-chain-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'original_decoder':'0x01a84f8c','decoder_calls':CASES*4,'decoded_controller_indices':[1,2,3,4],'original_callback':'0x019cce68','callback_end_exclusive':'0x019cd01c','original_render':'0x0197c870',
            'records':CASES,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,'pitch_buffer_words_compared':CASES*128,'active_audio_samples':(CASES-counts['render_inactive'])*128,
            'counts':counts,'routing_masks':len(masks),'active_channel_counts':sorted(active_counts),'source_controls':'0x01dff13c','volume_state':'0x01dd5b3c','pitch_buffer':'0xff801950','amplitude_buffer':'0xff801d50','amplitude_samples_compared':CASES*128,'routing_combinations':81,'base_banks_unchanged':True,
            'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with deferred D-register AND disjoint-halfword-load patches; not hardware validation',
            'limitations':['The complete callback runs unmodified in bounded volume/pitch/disabled routes. Instrument/ENV routes excluded. Volume smoothing is preserved across explicit streams; buffers are supplied each call.',
                           'Incoming pitch buffer is explicit and generally nonconstant; modifier consumes it. This is not the missing original VoiceCtl-register-to-buffer producer.',
                           'The event order and render counter remain explicit. L1 handoff, original clock, ADSR/output and plugin integration are not completed.',
                           'Firmware, original instructions, disassembly and private ELF are not distributed.']}
    report['patches']={str(x.relative_to(ROOT)):sha(x) for x in [templates/'gnu-13.1-dreg-writeback.patch',templates/'gnu-13.1-halfword-writeback.patch']}
    report['source_sha256']={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'Source/Voice246ControlInput.h',ROOT/'Source/ControlVolume246.h',ROOT/'Tests/Voice246ControlChainTests.cpp',ROOT/'Source/ControlPitch246.h',ROOT/'scripts/verify-control-volume246.py',templates/'voice-control-chain.s.in',templates/'parallel-halfword.s',Path(__file__).resolve()]}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original mixed volume/pitch-buffer routes',counts)
if __name__=='__main__':main()
