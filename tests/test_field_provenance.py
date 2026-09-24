"""Synthetic guest-state and parser checks, with no ROM or image fixtures."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audit_field_provenance import field_state, scheduler, lz77, span, archive_member, inspect_layers


class FieldProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.e=bytearray(0x40000)
        self.i=bytearray(0x8000)
        self.put32(self.i,0x699c,0x02000800)
        self.put32(self.i,0x6b54,0x0200e000)
        self.put32(self.e,0xa00,0x02000800)
        self.put16(self.e,0xa04,1)
        self.put16(self.e,0x800,0x8000)
        self.put32(self.e,0x808,0x08093995)
        self.put16(self.e,0xe000,1)

    @staticmethod
    def put16(data,off,value): struct.pack_into('<H',data,off,value)
    @staticmethod
    def put32(data,off,value): struct.pack_into('<I',data,off,value)

    def test_shared_free_field(self):
        state=field_state(self.e,self.i)
        self.assertTrue(state['owned'])
        self.assertEqual(state['control'],'free')

    def test_script_stays_owned_but_not_free_in_box_gaps(self):
        for flags in (0,4,5,0x1001,0x1005):
            self.put16(self.e,0xe000,flags)
            state=field_state(self.e,self.i)
            self.assertTrue(state['owned'])
            self.assertEqual(state['control'],'scripted-or-blocked')

    def test_unrelated_flags_do_not_hide_free_control(self):
        for flags in (1,0x21,0x41,0x81,0x101):
            self.put16(self.e,0xe000,flags)
            self.assertEqual(field_state(self.e,self.i)['control'],'free')

    def test_stale_field_ram_does_not_prove_scene(self):
        self.put32(self.e,0x808,0x0802b95d)
        self.assertFalse(field_state(self.e,self.i)['owned'])

    def test_suspended_retiring_replaced_or_dead_task_declines(self):
        for flags in (0,0x8800,0x8080,0x8040):
            self.put16(self.e,0x800,flags)
            self.assertFalse(field_state(self.e,self.i)['owned'])

    def test_unlinked_pool_record_is_not_owner(self):
        self.put32(self.e,0xa00,0)
        self.put16(self.e,0xa04,0)
        self.assertFalse(field_state(self.e,self.i)['owned'])

    def test_duplicate_owner_declines(self):
        self.put32(self.e,0x81c,0x02000820)
        self.put16(self.e,0x820,0x8000)
        self.put32(self.e,0x828,0x08093995)
        self.put32(self.e,0x838,0x02000800)
        self.put16(self.e,0xa04,2)
        self.assertFalse(field_state(self.e,self.i)['owned'])

    def test_bad_link_count_backlink_and_cycle(self):
        for offset,value in ((0x81c,0x02000800),(0x81c,0x02000801),
                             (0x81c,0x02000a00),(0x818,0x02000820),(0xa04,2)):
            self.setUp()
            self.put32(self.e,offset,value)
            with self.assertRaises(ValueError): scheduler(self.e,self.i)

    def test_invalid_root_and_short_memory(self):
        for value in (0,0x0200e001,0x03000000,0x0203ffff):
            self.put32(self.i,0x6b54,value)
            with self.assertRaises(ValueError): field_state(self.e,self.i)
        with self.assertRaises(ValueError): scheduler(self.e,b'')

    def test_checked_spans(self):
        self.assertEqual(span(b'abc',3,0),b'')
        for offset,size in ((-1,1),(0,-1),(3,1),(4,0)):
            with self.assertRaises(ValueError): span(b'abc',offset,size)

    @staticmethod
    def literal_lz(data):
        return b'\x10'+len(data).to_bytes(3,'little')+b''.join(b'\0'+data[x:x+8] for x in range(0,len(data),8))

    def test_lz_literals(self):
        data=bytes(range(32))
        self.assertEqual(lz77(self.literal_lz(data),0),data)

    def test_lz_invalid_streams(self):
        for data in (b'',b'\x11\x20\0\0',b'\x10\xff\xff\xff',
                     b'\x10\x20\0\0\x80\0\0',b'\x10\x20\0\0\0'):
            with self.assertRaises(ValueError): lz77(data,0)

    def test_archive_bounds(self):
        data=bytearray(64)
        self.put32(data,8,2)
        self.assertEqual(archive_member(data,0,0),32)
        for index in (-1,0xffff,100):
            with self.assertRaises(ValueError): archive_member(data,0,index)
        self.put32(data,8,0)
        with self.assertRaises(ValueError): archive_member(data,0,0)

    def test_source_provenance_not_geometry(self):
        rom=bytearray(4096)
        self.put32(self.i,0x2974,0x08000000)
        self.put32(rom,24,4)  # root member 2 -> archive 64
        self.put32(rom,72,4)  # archive member 0 -> resource 128
        data=bytearray(34)
        self.put16(data,16,0x4000)
        self.put16(data,20,8)
        self.put16(data,22,8)
        self.put32(data,28,32)
        self.put16(data,32,123)
        packed=self.literal_lz(data)
        rom[128:128+len(packed)]=packed
        for bg in range(1,4):
            d=0x2a20+bg*0x34
            self.put16(self.i,d,0x4000)
            self.put16(self.i,d+4,8)
            self.put16(self.i,d+6,8)
            self.put32(self.i,d+0x1c,0x02010000)
        self.put16(self.e,0x10000,123)
        state=field_state(self.e,self.i)
        self.assertTrue(all(x['authenticated'] for x in inspect_layers(self.e,self.i,rom,state)))
        self.put16(self.e,0x10000,124)
        self.assertTrue(all(not x['authenticated'] for x in inspect_layers(self.e,self.i,rom,state)))


if __name__=='__main__': unittest.main()
