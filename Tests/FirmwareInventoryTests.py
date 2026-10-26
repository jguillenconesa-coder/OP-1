# SPDX-License-Identifier: AGPL-3.0-only
"""Synthetic fixtures only: no Teenage Engineering firmware or assets."""
import importlib.util
import io
from contextlib import closing
import lzma
from pathlib import Path
import sqlite3
import struct
import tarfile
import tempfile
import unittest
import zlib
from functools import reduce

spec = importlib.util.spec_from_file_location('inventory', Path(__file__).resolve().parents[1]/'scripts/analyze-op1-firmware.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class InventoryTests(unittest.TestCase):
    def fixture(self, root, name='content/op1.db', link=False):
        db = root/'source.db'
        with closing(sqlite3.connect(db)) as conn:
            conn.execute('CREATE TABLE synth_slots(id INTEGER,patch TEXT)')
            conn.execute('INSERT INTO synth_slots VALUES(0,?)', ('{"name":"Synthetic","type":"fm"}',))
            conn.commit()
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            info = tarfile.TarInfo(name)
            if link:
                info.type = tarfile.SYMTYPE
                info.linkname = '/tmp/outside'
                archive.addfile(info)
            else:
                content = db.read_bytes()
                info.size = len(content)
                archive.addfile(info, io.BytesIO(content))
        compressed = lzma.compress(stream.getvalue(), format=lzma.FORMAT_ALONE)
        firmware = root/'synthetic.op1'
        firmware.write_bytes(struct.pack('<I', zlib.crc32(compressed))+compressed)
        return firmware

    def test_inventory_without_source_modification(self):
        with tempfile.TemporaryDirectory() as d:
            source = self.fixture(Path(d))
            before = source.read_bytes()
            report = module.inspect(source)
            self.assertEqual(before, source.read_bytes())
            self.assertTrue(report['container']['crc_verified'])
            self.assertEqual(report['counts']['regular_files'], 1)
            self.assertEqual(report['databases']['content/op1.db']['synth_slots']['slots'][0]['name'], 'Synthetic')
            self.assertNotIn('patch', str(report['databases']['content/op1.db']['synth_slots']['slots'][0]))

    def test_crc_failure(self):
        with tempfile.TemporaryDirectory() as d:
            source = self.fixture(Path(d))
            data = bytearray(source.read_bytes());data[-1] ^= 1;source.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'CRC'):
                module.inspect(source)

    def test_unsafe_paths_and_links(self):
        for name, link in [('../escape', False), ('/absolute', False), ('content/link', True)]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as d:
                source = self.fixture(Path(d), name, link)
                with self.assertRaises(ValueError):
                    module.inspect(source)

class LoaderMapTests(unittest.TestCase):
    def test_headers_fill_and_rejections(self):
        spec = importlib.util.spec_from_file_location('ldr_map', Path(__file__).resolve().parents[1]/'scripts/map-op1-ldr.py')
        mapper = importlib.util.module_from_spec(spec);spec.loader.exec_module(mapper)
        def block(address, payload, flags=0xad000006, count=None):
            header=bytearray(struct.pack('<4I', flags,address,len(payload) if count is None else count,0))
            header[2]^=reduce(lambda a,b:a^b,header,0)
            return bytes(header)+payload
        data=block(0x01000000,b'abcd')+block(0x01000004,b'',0xad000106,32)+block(0x01000008,b'ef')
        report,image=mapper.inspect(data)
        self.assertEqual(report['block_count'],3);self.assertEqual(report['fill_blocks'],1)
        self.assertEqual(report['mapped_payload_bytes'],6);self.assertEqual(image,b'abcd'+bytes(4)+b'ef')
        for invalid in [data[:-1],bytes([data[0]^1])+data[1:]]:
            with self.assertRaises(ValueError):mapper.inspect(invalid)

class FirmwareBankTests(unittest.TestCase):
    def test_synthetic_bank_pcm_roots_and_24_regions(self):
        import json
        import zipfile
        import wave
        spec=importlib.util.spec_from_file_location('firmware_bank',Path(__file__).resolve().parents[1]/'scripts/build-firmware-bank.py')
        bank=importlib.util.module_from_spec(spec);spec.loader.exec_module(bank)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);db=root/'factory.db'
            with closing(sqlite3.connect(db)) as conn:
                conn.execute('CREATE TABLE synth_presets(patch TEXT)');conn.execute('CREATE TABLE drum_presets(patch TEXT)')
                conn.execute('INSERT INTO synth_presets VALUES(?)',(json.dumps(dict(type='sampler',name='fixture',base_freq=440)),))
                q=lambda frame:round(frame*2**31/(44100*12))
                conn.execute('INSERT INTO drum_presets VALUES(?)',(json.dumps(dict(type='drum',name='kit',start=[q(i*10) for i in range(24)],end=[q(i*10+9) for i in range(24)])),));conn.commit()
            pcm=b''.join(struct.pack('<h',i*100-12000) for i in range(240));raw=io.BytesIO()
            with tarfile.open(fileobj=raw,mode='w') as archive:
                for name,content in [('content/op1_factory.db',db.read_bytes()),('content/audio/factory_synth/fixture.raw',pcm),('content/audio/factory_drum/kit.raw',pcm)]:
                    info=tarfile.TarInfo(name);info.size=len(content);archive.addfile(info,io.BytesIO(content))
            compressed=lzma.compress(raw.getvalue(),format=lzma.FORMAT_ALONE);source=root/'fixture.op1';source.write_bytes(struct.pack('<I',zlib.crc32(compressed))+compressed);target=root/'bank.zip'
            report=bank.build(source,target,44100);self.assertEqual(report['presets'],2);self.assertEqual(report['regions'],24)
            with zipfile.ZipFile(target) as archive:
                manifest=json.loads(archive.read('bank.json'));sample,kit=manifest['presets'];self.assertEqual(sample['parameters']['sampleRoot'],69);self.assertEqual(sample['parameters']['sampleClean'],1);self.assertEqual(len(kit['kit']),24)
                with wave.open(io.BytesIO(archive.read(kit['kit'][0]))) as audio:self.assertEqual(audio.readframes(10),pcm[:20]);self.assertEqual(audio.getframerate(),44100)
            with self.assertRaises(ValueError):bank.drum_region(100,10,240,44100)

if __name__ == '__main__':
    unittest.main()
