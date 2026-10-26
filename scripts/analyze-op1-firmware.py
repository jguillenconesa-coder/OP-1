#!/usr/bin/env python3
"""Read-only OP-1 container inventory. Emits metadata, never firmware/resources.

Usage: python scripts/analyze-op1-firmware.py firmware.op1 report.json
No device access, target execution, extraction to user paths, or repacking.
"""
import argparse
import collections
import hashlib
import io
import json
import lzma
from pathlib import Path, PurePosixPath
import re
import sqlite3
import struct
import tempfile
import zlib

LIMIT = 128 * 1024 * 1024

def inspect(path):
    if not 17 <= path.stat().st_size <= LIMIT:
        raise ValueError('Firmware must be between 17 bytes and 128 MiB')
    data = path.read_bytes()
    if len(data) > LIMIT:
        raise ValueError('Firmware grew beyond the input limit')
    crc, = struct.unpack('<I', data[:4])
    if crc != zlib.crc32(data[4:]):
        raise ValueError('Container CRC mismatch')
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE, memlimit=LIMIT)
    payload = decoder.decompress(data[4:], max_length=LIMIT + 1)
    if len(payload) > LIMIT or not decoder.eof or decoder.unused_data:
        raise ValueError('Incomplete, trailing or oversized LZMA payload')
    report = dict(format='Pocket One OP-1 static inventory v1',
                  firmware=dict(filename=path.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()),
                  container=dict(crc32=f'{crc:08x}', crc_verified=True, lzma_offset=4,
                                 tar_bytes=len(payload), tar_sha256=hashlib.sha256(payload).hexdigest()),
                  files=[], databases={}, code={})
    import tarfile
    total = 0
    with tarfile.open(fileobj=io.BytesIO(payload)) as archive:
        names = set()
        for i, member in enumerate(archive):
            if i >= 10000:
                raise ValueError('Too many archive entries')
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts or '\\' in member.name or member.name in names:
                raise ValueError('Unsafe or duplicate member name')
            names.add(member.name)
            if member.isdir():
                continue
            if not member.isfile():
                raise ValueError('Links and special archive entries are not supported')
            total += member.size
            if member.size > LIMIT or total > LIMIT:
                raise ValueError('Archive exceeds the inventory budget')
            content = archive.extractfile(member).read(LIMIT + 1)
            if len(content) != member.size:
                raise ValueError('Truncated archive member')
            report['files'].append(dict(path=member.name, bytes=len(content), sha256=hashlib.sha256(content).hexdigest()))
            if member.name == 'OP1_vdk.ldr':
                banners = sorted(set(x.decode('ascii') for x in re.findall(rb'Rev\.[^\x00\n]{0,100}', content)))
                report['code'] = dict(banners=banners, format='LDR; machine instructions not decoded',
                                      dsp_algorithms_verified=False)
            if member.name.endswith('.db'):
                with tempfile.TemporaryDirectory(prefix='pocket-firmware-db-') as directory:
                    db = Path(directory)/'op1.db'
                    db.write_bytes(content)
                    with sqlite3.connect(db.as_uri()+'?mode=ro', uri=True) as conn:
                        conn.execute('PRAGMA trusted_schema=OFF')
                        conn.execute('PRAGMA query_only=ON')
                        conn.setlimit(sqlite3.SQLITE_LIMIT_LENGTH, 8 * 1024 * 1024)
                        tables = {name for name, in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")}
                        database = {}
                        report['databases'][member.name] = database
                        database['tables'] = sorted(tables)
                        for table in ['synth_presets', 'drum_presets', 'synth_types', 'drum_types', 'fx_types', 'lfo_types', 'seq_types', 'synth_slots', 'drum_slots', 'endless', 'drumseq', 'okseq', 'sequencer', 'knobs', 'master']:
                            if table not in tables:
                                continue
                            row = dict(rows=conn.execute('SELECT count(*) FROM "'+table+'"').fetchone()[0])
                            if table in ('synth_slots', 'drum_slots'):
                                row['slots'] = []
                                for slot, patch in conn.execute('SELECT id,patch FROM "'+table+'" LIMIT 128'):
                                    patch = json.loads(patch)
                                    entry = dict(slot=slot, name=patch.get('name'), type=patch.get('type'),
                                                 version=patch.get('synth_version', patch.get('drum_version')),
                                                 fields=sorted(patch.keys()))
                                    if 'base_freq' in patch:
                                        entry['base_freq'] = patch['base_freq']
                                    row['slots'].append(entry)
                            if table in ('endless', 'drumseq', 'okseq'):
                                row['structures'] = []
                                for is_synth, text in conn.execute('SELECT is_synth,data FROM "'+table+'" LIMIT 128'):
                                    value = json.loads(text)
                                    row['structures'].append(dict(is_synth=bool(is_synth), fields=sorted(value.keys()),
                                                                 array_lengths={key:len(v) for key,v in value.items() if isinstance(v,list)}))
                            if table.endswith('_types'):
                                row['types'] = [name for name, in conn.execute('SELECT type FROM "'+table+'" LIMIT 128')]
                            if table.endswith('_presets'):
                                parsed = [json.loads(p) for p, in conn.execute('SELECT patch FROM "'+table+'" LIMIT 1024')]
                                row['by_type'] = dict(collections.Counter(p.get('type', 'unknown') for p in parsed))
                                row['names'] = [dict(name=p.get('name'), type=p.get('type')) for p in parsed]
                            database[table] = row
    report['counts'] = dict(regular_files=len(report['files']),
                            by_extension=dict(collections.Counter(PurePosixPath(x['path']).suffix for x in report['files'])),
                            factory_synth=sum(x['path'].startswith('content/audio/factory_synth/') for x in report['files']),
                            factory_drum=sum(x['path'].startswith('content/audio/factory_drum/') for x in report['files']))
    report['limitations'] = ['Static structure only; no original DSP execution or audio equivalence established.',
                             'No firmware, audio, graphics, source reconstruction or patch parameter arrays are emitted.',
                             'Raw audio sample rates and fixed-point control scaling are not inferred from filenames.']
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    report = inspect(args.firmware)
    # Exclusive publication protects existing findings and the source file.
    with args.report.open('x', encoding='utf-8') as output:
        json.dump(report, output, indent=2, ensure_ascii=False)
        output.write('\n')
    print(f"CRC verified; inventoried {report['counts']['regular_files']} files; DSP remains unverified")

if __name__ == '__main__':
    main()
