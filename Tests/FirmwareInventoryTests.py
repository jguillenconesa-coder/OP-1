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

if __name__ == '__main__':
    unittest.main()
