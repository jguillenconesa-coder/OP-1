#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Contrast all 16-bit Dr. Wave dispatch words and full-render boundary streams.

Only the consumer is verified. No encrypted L1 writer or original clock is run.
Original instructions, images, ELF and expanded render inputs stay private.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RECORD, RESULT, CASES, BANK = 9316, 8784, 512, 163840

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def overrides(base):
    data = bytearray()
    edges = (32765, 32766, 32767, 32768, 32769, 32770, 65532, 65533, 65534, 65535, 0, 1, 2, 3, 4, 5)
    for case in range(CASES):
        action = struct.unpack_from('<I', base, case * RECORD)[0]
        previous = struct.unpack_from('<I', base, case * RECORD + 8)[0]
        if case < 256:
            slot, counter = case % 6, edges[(case // 6) % len(edges)]
        else:
            offset = (case - 256) % 128
            slot = offset % 6
            # Explicit test clock: six calls share a word. Not inferred cadence.
            start = 32758 if case < 384 else 65526
            counter = (start + offset // 6) & 65535
            # The source dataset reloads every 64 calls; these streams span 128.
            # Preserve constructor/reset/release events but reload only at start.
            action = (action & ~1) | int(offset == 0)
        data += struct.pack('<3I', action, slot, (previous & 0xffff0000) | counter)
    return data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['ldr', 'assembler', 'linker', 'simulator', 'testutils', 'output']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() or out.is_relative_to(ROOT):
        raise ValueError('Use a NEW private directory outside checkout')
    expected = json.loads((ROOT / 'docs/firmware/op1-246-ldr-map.json').read_text())['sha256']
    if sha(args.ldr) != expected:
        raise ValueError('Only inventoried rev246 is supported')
    base_path = ROOT / 'Tests/ReferenceVectors/drwave246-kernel-input.bin'
    base = base_path.read_bytes()
    if len(base) != CASES * RECORD:
        raise ValueError('Wrong source fixture size')
    spec = importlib.util.spec_from_file_location('ldr_map', ROOT / 'scripts/map-op1-ldr.py')
    mapper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mapper)
    raw = args.ldr.read_bytes()
    meta, _ = mapper.inspect(raw)
    address_base = int(meta['sdram_start'], 16)
    image = bytearray(int(meta['sdram_end'], 16) - address_base)
    cursor = 0
    while cursor < len(raw):
        flags, address, size, argument = struct.unpack_from('<4I', raw, cursor)
        if address_base <= address and address + size <= address_base + len(image) and not flags & 0x1000:
            image[address - address_base:address - address_base + size] = (
                (struct.pack('<I', argument) * ((size + 3) // 4))[:size]
                if flags & 0x100 else raw[cursor + 16:cursor + 16 + size])
        cursor += 16 + (0 if flags & 0x100 else size)
    out.mkdir(parents=True)
    (out / 'sdram.bin').write_bytes(image)
    changes = overrides(base)
    expanded = bytearray(base)
    for case in range(CASES):
        expanded[case * RECORD:case * RECORD + 12] = changes[case * 12:case * 12 + 12]
    (out / 'overrides.bin').write_bytes(changes)
    (out / 'inputs.bin').write_bytes(expanded)
    templates = ROOT / 'research/blackfin'

    def run(command, **kw):
        subprocess.run([str(x) for x in command], check=True, timeout=180, **kw)

    def probe(template, name):
        source = (templates / template).read_text().replace('@WORK@', out.as_posix()).replace('@BYTES@', str(CASES * RESULT))
        (out / (name + '.s')).write_text(source)
        run([args.assembler, '-I', args.testutils, '-o', out / (name + '.o'), out / (name + '.s')])
        run([args.linker, '-T', templates / 'voice-contract.ld', '-o', out / (name + '.elf'), out / (name + '.o')])
        with (out / (name + '.bin')).open('wb') as stdout, (out / (name + '.log')).open('wb') as stderr:
            run([args.simulator, '--model', 'bf524', '--memory-size', '64M',
                 '--memory-region', '0xff800000,0x8000', '--memory-region', '0xff900000,0x8000',
                 '--watch-cycles-int', '900000000', out / (name + '.elf')], stdout=stdout, stderr=stderr)

    probe('drwave-counter.s.in', 'counter')
    observed = (out / 'counter.bin').read_bytes()
    if len(observed) != 131072 * 8:
        raise RuntimeError('Incomplete counter output')
    for case in range(131072):
        result, preserved = struct.unpack_from('<2I', observed, case * 8)
        word = case & 65535
        expected_word = word | (0xffff0000 if case >= 65536 else 0)
        if result != word % 6 or preserved != expected_word:
            raise RuntimeError(f'Original counter mismatch at {case}')
    probe('drwave-kernel.s.in', 'render')
    output = (out / 'render.bin').read_bytes()
    if len(output) != BANK * 2 + CASES * RESULT or output[:BANK] != output[-BANK:]:
        raise RuntimeError('Incomplete render or changed base banks')
    artifacts = {}
    for name, path in [('drwave246-counter-output.bin', out / 'counter.bin'),
                       ('drwave246-counter-overrides.bin', out / 'overrides.bin'),
                       ('drwave246-counter-render-output.bin', out / 'render.bin')]:
        fixture = ROOT / 'Tests/ReferenceVectors' / name
        if args.record:
            fixture.write_bytes(path.read_bytes())
        if fixture.read_bytes() != path.read_bytes():
            raise RuntimeError('Counter oracle differs: ' + name)
        artifacts[name] = {'bytes': path.stat().st_size, 'sha256': sha(path)}
    counts = {'updated': 0, 'read_only': 0, 'inactive': 0}
    high_active = high_updated = 0
    for case in range(CASES):
        _, slot, counter, enabled = struct.unpack_from('<4I', expanded, case * RECORD)
        key = 'inactive' if not enabled else 'updated' if slot == (counter & 65535) % 6 else 'read_only'
        counts[key] += 1
        high_active += int(enabled != 0 and counter & 0x8000 != 0)
        high_updated += int(key == 'updated' and counter & 0x8000 != 0)
    report = {
        'ldr_sha256': expected, 'source_fixture_sha256': sha(base_path),
        'original_dispatch_fragment': ['0x0197c89a', '0x0197c8ca'],
        'fragment_end_exclusive': True, 'exhaustive_counter_words': 65536,
        'neighbor_halfwords': [0, 65535], 'fragment_calls': 131072,
        'original_complete_render': '0x0197c870', 'render_records': CASES,
        'counts': counts, 'audio_samples': (CASES - counts['inactive']) * 128,
        'high_bit_active_calls': high_active, 'high_bit_wave_updates': high_updated,
        'independent_cases': 256, 'explicit_streams': 2, 'calls_per_stream': 128,
        'stream_start_words': [32758, 65526], 'test_calls_per_counter_word': 6,
        'base_banks_unchanged': True, 'artifacts': artifacts,
        'tools': {str(x): sha(x) for x in [args.assembler, args.linker, args.simulator]},
        'model': 'GNU BF524 with documented D-register deferred-writeback patch; not hardware validation',
        'limitations': [
            'Fragment output is only the counter consumer, not the full render. The 512-call second oracle runs the unmodified complete render.',
            'The six calls per test word and its increment/wrap are caller inputs, not verified original cadence.',
            'Candidate L1 writer is partly encrypted. No original scheduler, allocator, pitch calibration/provider, ADSR, output or plugin integration claimed.',
            'The adapter accepts int counters 0..65535, rejecting out-of-domain ints. Only low16 of the original counter DWORD are consumed; upper16 remain unchanged.'
        ]
    }
    (out / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: 131072 dispatch fragments; complete render', counts)

if __name__ == '__main__':
    main()
