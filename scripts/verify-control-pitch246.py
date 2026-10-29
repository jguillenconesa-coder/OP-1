#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast a readable pitch-buffer modifier and the following Dr. Wave render.
The initial buffer, controls and event order remain explicit. This does not run
the partially encrypted original register-to-buffer handoff.
"""
import argparse,hashlib,importlib.util,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES,BASE,EXTRA,RECORD,RESULT,BANK=512,9316,108,9424,9404,163840
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs(base):
    seed=0x43504954
    def rnd():
        nonlocal seed
        seed=(seed*1664525+1013904223)&0xffffffff
        return seed
    raw_values=(0,1,2,63,64,65,126,127,128,255,32767,32768,65535)
    data=bytearray();expanded=bytearray()
    for case in range(CASES):
        mask=case%16 if case<256 else rnd()%16
        kind=2 if case%11==0 else 0
        targets=[4096+(rnd()%4096) if mask>>i&1 else rnd()%65536-32768 for i in range(4)]
        routing=[(6144 if mask>>i&1 else 4096)+rnd()%2048 for i in range(4)]
        cv=[(rnd()&0xffff0000)|raw_values[(case//16+i)%len(raw_values)] if case<256 else rnd() for i in range(4)]
        source_image=[rnd(),rnd(),*cv,rnd(),rnd()]
        smooth=[rnd() for _ in range(6)];self_image=[rnd() for _ in range(4)];guards=[rnd() for _ in range(4)]
        row=struct.pack('<I8h8I6I4I4I',kind,*targets,*routing,*source_image,*smooth,*self_image,*guards)
        data+=row;expanded+=base[case*BASE:(case+1)*BASE]+row
    if len(data)!=CASES*EXTRA or len(expanded)!=CASES*RECORD:raise RuntimeError('Wrong control-pitch dimensions')
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
    out.mkdir(parents=True);changes,expanded=inputs(base)
    (out/'sdram.bin').write_bytes(image);(out/'inputs.bin').write_bytes(expanded);(out/'overrides.bin').write_bytes(changes)
    templates=ROOT/'research/blackfin'
    (out/'probe.s').write_text((templates/'control-pitch246.s.in').read_text().replace('@WORK@',out.as_posix()).replace('@BYTES@',str(CASES*RESULT)))
    def run(cmd,**kw):subprocess.run([str(x) for x in cmd],check=True,timeout=180,**kw)
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
    for name,path in [('control-pitch246-overrides.bin',out/'overrides.bin'),('control-pitch246-output.bin',out/'output.bin')]:
        target=ROOT/'Tests/ReferenceVectors'/name
        if a.record:target.write_bytes(path.read_bytes())
        if target.read_bytes()!=path.read_bytes():raise RuntimeError('Oracle differs: '+name)
        artifacts[name]={'bytes':path.stat().st_size,'sha256':sha(path)}
    report={'ldr_sha256':expected,'source_fixture_sha256':sha(basepath),'original_callback':'0x019cce68','callback_end_exclusive':'0x019cd01c','original_render':'0x0197c870',
            'records':CASES,'independent_cases':256,'continuous_streams':4,'calls_per_stream':64,'pitch_buffer_words_compared':CASES*128,'active_audio_samples':(CASES-counts['render_inactive'])*128,
            'counts':counts,'routing_masks':len(masks),'active_channel_counts':sorted(active_counts),'source_controls':'0x01dff13c','volume_state':'0x01dd5b3c','pitch_buffer':'0xff801950','base_banks_unchanged':True,
            'artifacts':artifacts,'tools':{str(x):sha(x) for x in [a.assembler,a.linker,a.simulator]},'model':'GNU BF524 with documented deferred D-register patch; not hardware validation',
            'limitations':['The complete callback runs unmodified only in its bounded pitch/disabled routes, with other destinations excluded. No complete LFO or module identity is inferred.',
                           'Incoming pitch buffer is explicit and generally nonconstant; modifier consumes it. This is not the missing original VoiceCtl-register-to-buffer producer.',
                           'The event order and render counter remain explicit. L1 handoff, original clock, ADSR/output and plugin integration are not completed.',
                           'Firmware, original instructions, disassembly and private ELF are not distributed.']}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: original control pitch-buffer route',counts)
if __name__=='__main__':main()
