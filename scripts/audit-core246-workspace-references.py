#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Inventory bounded workspace address candidates; never execute/disassemble L1.
Reports addresses and assumptions only. Does not identify a per-call producer.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ldr',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Use a NEW private output directory outside checkout')
    raw=a.ldr.read_bytes();digest=hashlib.sha256(raw).hexdigest();expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if digest!=expected:raise ValueError('Unsupported LDR')
    spec=importlib.util.spec_from_file_location('mapper',ROOT/'scripts/map-op1-ldr.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);meta,image=m.inspect(raw);base=int(meta['sdram_start'],16)
    def lows(b):return [i for i in range(0,len(b)-3,2) if b[i+1]==0xe1 and b[i+2:i+4]==b'\x24\x17']
    def pair(b,i):return i+8<=len(b) and b[i+4]==b[i]+0x40 and b[i+5:i+8]==b'\xe1\x80\xff'
    def refs(target):
        needle=struct.pack('<I',target);return [hex(base+i) for i in range(0,len(image)-3,4) if image[i:i+4]==needle]
    sdram=[{'address':hex(base+i),'adjacent_high_ff80_pair':pair(image,i)} for i in lows(image)]
    cursor=0;blocks=[]
    while cursor<len(raw):
        flags,address,n,arg=struct.unpack_from('<4I',raw,cursor)
        if address==0xffa00000 and n==49076:
            b=raw[cursor+16:cursor+16+n];candidates=[]
            for i in lows(b):
                if i%24<8 or i%24+4>24:continue
                group=i-i%24;visible=i%24+8<=24
                candidates.append({'address':hex(address+i),'complete_pair_visible':visible,'adjacent_high_ff80_pair':pair(b,i) if visible else None,'preceding_protected_span':[hex(address+group),hex(address+group+8)],'following_protected_span':[hex(address+group+24),hex(address+group+32)]})
            blocks.append({'header_offset':hex(cursor),'payload_offset':hex(cursor+16),'address':hex(address),'bytes':n,'flags':hex(flags),'candidates':candidates})
        cursor+=16+(0 if flags&0x100 else n)
    report={'ldr_sha256':digest,'workspace':'0xff801724','sdram_immediate_candidates':sdram,'initializer_pointer_references':refs(0x01968774),'callback_pointer_references':refs(0x019cce68),'l1_blocks':blocks,'protection_assumption':'Previously documented community profile: first 8 bytes of each 24-byte group are protected; no L1 instruction execution or decryption.','limits':['Immediate/pointer matches are candidates, not a control-flow proof or exhaustive writer search; computed addresses can be missed.','Visible L1 address loads do not establish their caller, copy source, write direction or cadence. The third candidate lacks a visible complete high-half pair.','Initializer writes zeros but does not recover the per-call working-image producer. No keys/hardware assumed or requested.']}
    out.mkdir(parents=True,mode=0o700);(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS: workspace address inventory; no L1 execution/disassembly')
if __name__=='__main__':main()
