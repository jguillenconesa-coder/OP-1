#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Check existing private probe ELFs against the separate deferred-register model.
First generate --probes with verify-reference-dsp.py using stock GNU 13.1.
No original instructions are copied into this repository or the output JSON.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probes',required=True,type=Path);p.add_argument('--simulator',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--clock-probes',type=Path,help='Optional private probes from verify-phase-contract.py')
    a=p.parse_args();source=a.probes.resolve();out=a.output.resolve()
    if source.is_relative_to(ROOT) or out.exists():raise ValueError('Probes must be private; choose a new JSON output')
    expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256'];old=json.loads((source/'evidence.json').read_text())
    if old['ldr_sha256']!=expected:raise ValueError('Wrong probe firmware profile')
    # Check every recorded input/output before executing the privately generated ELFs.
    fixture_to_path={'fm-fields.bin':'fm-probe/results.bin','q15-input.bin':'q15-probe/inputs.bin','q15-output.bin':'q15-probe/results.bin','pd-input.bin':'pd-probe/inputs.bin','pd-output.bin':'pd-probe/results.bin','synthetic-sine.bin':'pd-probe/table.bin','pd-transient-input.bin':'pd-full-probe/inputs.bin','pd-transient-output.bin':'pd-full-probe/results.bin','verified-sine.bin':'pd-full-probe/table.bin','lut-copy-output.bin':'lut-probe/results.bin','pd-stream-input.bin':'pd-stream-probe/inputs.bin','pd-stream-output.bin':'pd-stream-probe/results.bin'}
    for name,relative in fixture_to_path.items():
        if sha(source/relative)!=old['artifacts'][name]['sha256'] or (source/relative).read_bytes()!=(ROOT/'Tests/ReferenceVectors'/name).read_bytes():raise ValueError('Private probes/fixtures differ: '+name)
    results={}
    for folder,fixture,regions in [('fm-probe','fm-fields.bin',['0xff904000,0x4000']),('q15-probe','q15-output.bin',[]),('pd-probe','pd-output.bin',['0xff800000,0x8000']),('pd-full-probe','pd-transient-output.bin',['0xff800000,0x8000']),('pd-stream-probe','pd-stream-output.bin',['0xff800000,0x8000']),('lut-probe','verified-sine.bin',[])]:
        elf=source/folder/'probe.elf';command=[str(a.simulator),'--model','bf524','--memory-size','64M','--watch-cycles-int','300000000']
        for region in regions:command+=['--memory-region',region]
        data=subprocess.check_output(command+[str(elf)],timeout=60)
        if data!=(ROOT/'Tests/ReferenceVectors'/fixture).read_bytes():raise RuntimeError('Deferred-model mismatch: '+folder)
        results[folder]={'elf_sha256':sha(elf),'expected_fixture':fixture,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'matches':True}
    report={'ldr_sha256':expected,'model':'Local GNU 13.1 deferred D-register patch, not hardware validation','simulator_sha256':sha(a.simulator),'patch_sha256':sha(ROOT/'research/blackfin/gnu-13.1-dreg-writeback.patch'),'pd_samples_unchanged':1474560,'fm_fields_unchanged':65536,'q15_pairs_unchanged':65536,'lut_copy_matches_verified_unshifted_source':True,'stock_fixtures_preserved':True,'probes':results}
    if a.clock_probes:
        clock=a.clock_probes.resolve();clock_meta=json.loads((clock/'evidence.json').read_text())
        if clock.is_relative_to(ROOT) or clock_meta['ldr_sha256']!=expected:raise ValueError('Wrong private clock-probe profile')
        if (clock/'pd-stream-probe/inputs.bin').read_bytes()!=(ROOT/'Tests/ReferenceVectors/pd-clock-input.bin').read_bytes():raise ValueError('Clock input differs')
        elf=clock/'pd-stream-probe/probe.elf'
        data=subprocess.check_output([str(a.simulator),'--model','bf524','--memory-size','64M','--memory-region','0xff800000,0x8000','--watch-cycles-int','300000000',str(elf)],timeout=60)
        if data!=(ROOT/'Tests/ReferenceVectors/pd-clock-output.bin').read_bytes():raise RuntimeError('Clock output differs')
        report['probes']['pd-clock-probe']={'elf_sha256':sha(elf),'expected_fixture':'pd-clock-output.bin','bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'matches':True}
        report['pd_samples_unchanged']+=98304
    with out.open('x') as f:json.dump(report,f,indent=2);f.write('\n')
    print('PASS: all original Pd/FM/Q15 results preserved under deferred writes; original LUT copy now matches the unshifted source table')
if __name__=='__main__':main()
