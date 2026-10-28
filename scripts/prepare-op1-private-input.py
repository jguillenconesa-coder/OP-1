#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Extract only the inventoried rev246 LDR into a NEW private directory.
Accepts the user's original .op1 or an already extracted matching LDR.
No resource-bank extraction, upload, firmware execution or repo mutation.
"""
import argparse
import hashlib
import io
import json
import lzma
import os
from pathlib import Path
import struct
import tarfile
import zlib

ROOT=Path(__file__).resolve().parents[1]
LIMIT=128*1024*1024
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();out=args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Choose a NEW private output directory outside checkout')
    if not 17<=args.source.stat().st_size<=LIMIT:raise ValueError('Input size outside profile')
    data=args.source.read_bytes();digest=hashlib.sha256(data).hexdigest()
    ldr_expected=json.loads((ROOT/'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    firmware_expected=json.loads((ROOT/'docs/firmware/op1-246-inventory.json').read_text())['firmware']['sha256']
    if digest==ldr_expected:ldr=data
    elif digest==firmware_expected:
        if struct.unpack_from('<I',data)[0]!=zlib.crc32(data[4:]):raise ValueError('Container CRC mismatch')
        decoder=lzma.LZMADecompressor(format=lzma.FORMAT_ALONE,memlimit=LIMIT)
        payload=decoder.decompress(data[4:],max_length=LIMIT+1)
        if not decoder.eof or decoder.unused_data or len(payload)>LIMIT:raise ValueError('Invalid LZMA profile')
        with tarfile.open(fileobj=io.BytesIO(payload)) as source:
            matches=[member for member in source.getmembers() if member.name=='OP1_vdk.ldr']
            if len(matches)!=1 or not matches[0].isfile() or not 0<matches[0].size<=LIMIT:raise ValueError('Invalid LDR member')
            with source.extractfile(matches[0]) as f:ldr=f.read(LIMIT+1)
    else:raise ValueError('Source is not the inventoried rev246 .op1 or LDR')
    if hashlib.sha256(ldr).hexdigest()!=ldr_expected:raise ValueError('Extracted LDR SHA256 mismatch')
    os.umask(0o077);out.mkdir(parents=True,mode=0o700)
    target=out/'OP1_vdk.ldr';target.write_bytes(ldr)
    (out/'private-input.json').write_text(json.dumps({'source_sha256':digest,'ldr_sha256':ldr_expected,'scope':'Local private input, not a public artifact'},indent=2)+'\n')
    print(target)
if __name__=='__main__':main()
