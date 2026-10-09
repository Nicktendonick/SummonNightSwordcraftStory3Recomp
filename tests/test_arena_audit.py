"""Synthetic safety gates for private arena test-state selection."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audit_release105_arenas import SHA1, sections, selection_offset


class ArenaFixtureTests(unittest.TestCase):
    def fixture(self,pointer=0x02002000):
        b=bytearray(b'GBAS'+struct.pack('<I',2)+SHA1.encode()+struct.pack('<I',1))
        b.extend(b'BUS0'+struct.pack('<I',0x48000)+bytes(0x48000))
        i=60+0x40000
        struct.pack_into('<I',b,i+0x6ac0,0x03000000)
        struct.pack_into('<I',b,i+0x6ab4,2)
        struct.pack_into('<I',b,i+0x6584,pointer)
        return b

    def test_selects_exact_script_variable_in_each_ram_region(self):
        for pointer,expected in ((0x02002000,60+0x2021),(0x03006000,60+0x40000+0x6021)):
            b=self.fixture(pointer); original=bytes(b)
            offset=selection_offset(b)
            self.assertEqual(offset,expected)
            b[offset]=16
            self.assertEqual([n for n,(a,c) in enumerate(zip(original,b)) if a!=c],[offset])

    def test_rejects_wrong_rom(self):
        b=self.fixture(); b[8]=ord('f')
        with self.assertRaises(AssertionError): selection_offset(b)

    def test_rejects_already_loaded_battle_or_stale_root(self):
        for offset,value in ((0x6ab4,3),(0x6ab4,4),(0x6ab4,0),(0x6ac0,0x55555555)):
            b=self.fixture(); struct.pack_into('<I',b,60+0x40000+offset,value)
            with self.assertRaises(AssertionError): selection_offset(b)

    def test_rejects_pointer_outside_ram_including_crossing_end(self):
        for pointer in (0,0x08000000,0x0203fff0,0x03007ff0,0xffffffff):
            with self.assertRaises(AssertionError): selection_offset(self.fixture(pointer))

    def test_rejects_truncation_and_unframed_trailing_bytes(self):
        b=self.fixture()
        for bad in (b[:-1],b+b'junk',b[:49]):
            with self.assertRaises((AssertionError,struct.error)): sections(bad)


if __name__=='__main__': unittest.main()
