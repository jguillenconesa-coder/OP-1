#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Inventory a candidate Blackfin LDR stream without executing it.

Writes metadata only. --private-image optionally writes a local SDRAM candidate
for an external disassembler; never include that proprietary image in a release.
This inferred loader profile does not model boot execution or overlays.
"""
import argparse
from collections import Counter
from functools import reduce
import hashlib
import json
from pathlib import Path
import re
import struct

LIMIT=32*1024*1024

def inspect(data):
    if not data or len(data)>LIMIT:
        raise ValueError('LDR input must be nonempty and at most 32 MiB')
    offset=0;blocks=[];memory=bytearray(0x1000000);written=bytearray(0x1000000);low=0x02000000;high=0x01000000
    while offset<len(data):
        if len(blocks)>=10000 or offset+16>len(data):raise ValueError('Truncated or excessive headers')
        header=data[offset:offset+16]
        flags,address,count,argument=struct.unpack('<4I',header)
        if flags>>24!=0xad or reduce(lambda a,b:a^b,header,0):raise ValueError('Header signature/XOR mismatch')
        fill=bool(flags&0x100);payload_size=0 if fill else count
        if count>LIMIT or payload_size>len(data)-offset-16 or address+count>2**32:raise ValueError('Invalid block extent')
        blocks.append(dict(header_offset=offset,address=f'{address:#010x}',bytes=count,flags=f'{flags:#010x}',argument=f'{argument:#010x}',fill=fill))
        if payload_size and 0x01000000<=address and address+count<=0x02000000:
            start=address-0x01000000;memory[start:start+count]=data[offset+16:offset+16+count];written[start:start+count]=b"\x01"*count;low=min(low,address);high=max(high,address+count)
        offset+=16+payload_size
    if high<=low:raise ValueError('No payload in the selected SDRAM window')
    image=memory[low-0x01000000:high-0x01000000]
    hints={}
    for match in re.finditer(rb'\.\.\\src\\dsp\\([^\x00:]{1,100}):',image):
        name=match.group(1).decode('ascii',errors='replace')
        hints.setdefault(name,[]).append(hex(low+match.start()))
    report=dict(profile='Inferred AD-signature 16-byte LDR headers; flag 0x100 has no payload',
                sha256=hashlib.sha256(data).hexdigest(),input_bytes=len(data),consumed_bytes=offset,
                block_count=len(blocks),all_header_xors_zero=True,fill_blocks=sum(b['fill'] for b in blocks),
                mapped_payload_bytes=written.count(1),sdram_start=hex(low),sdram_end=hex(high),
                flags=dict(Counter(b['flags'] for b in blocks)),dsp_source_name_hints=hints,
                limitations=['Framing verified for this input, boot semantics/overlays not emulated.',
                             'SDRAM candidate applies payload writes in file order, omits fills and other address regions.',
                             'Missing addresses are zero placeholders, not observed firmware bytes.',
                             'Source-name strings do not establish function boundaries or recovered source.'])
    return report,bytes(image)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ldr',type=Path);parser.add_argument('report',type=Path)
    parser.add_argument('--private-image',type=Path)
    args=parser.parse_args()
    if args.ldr.stat().st_size>LIMIT:raise ValueError('Input exceeds 32 MiB')
    report,image=inspect(args.ldr.read_bytes())
    if args.report.exists() or (args.private_image and args.private_image.exists()):raise ValueError('Select new output paths')
    with args.report.open('x',encoding='utf-8') as output:json.dump(report,output,indent=2);output.write('\n')
    if args.private_image:
        with args.private_image.open('xb') as output:output.write(image)
    print(f"{report['block_count']} headers verified; metadata written; DSP not executed")

if __name__=='__main__':main()
