"""Synthetic framing safety tests; no ROMs, player saves or game process.

Payloads here exercise only the audit's container parser. They are deliberately
not complete subsystem serializations and must never be passed to the runtime.
"""
import hashlib
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_translation106_inputs import NEW_SHA1, OLD_SHA1, REQUIRED_SECTIONS, read_snapshot


class Translation106FixtureGateTests(unittest.TestCase):
    @staticmethod
    def fixture(bus_size=0x48000):
        blob = bytearray(b'GBAS' + struct.pack('<I', 2) + OLD_SHA1.encode('ascii')
                         + struct.pack('<I', 7))
        offsets = {}
        for tag in (b'CPU0', b'BUS0', b'IO_0', b'AUD0', b'SAV0', b'PPU0', b'META'):
            offsets[tag] = len(blob)
            payload = bytes(bus_size) if tag == b'BUS0' else b'\x12\x34\x56'
            blob.extend(tag + struct.pack('<I', len(payload)) + payload)
        return blob, offsets

    def test_accepts_exact_seven_section_structure(self):
        blob, offsets = self.fixture()
        sections = read_snapshot(blob, OLD_SHA1)
        self.assertEqual(set(sections), REQUIRED_SECTIONS)
        self.assertEqual(sections[b'BUS0']['size'], 0x48000)
        for tag, entry in sections.items():
            self.assertEqual(entry['offset'], offsets[tag] + 8)
            payload = blob[entry['offset']:entry['offset'] + entry['size']]
            self.assertEqual(entry['sha256'], hashlib.sha256(payload).hexdigest())

    def test_header_only_retag_preserves_every_section_hash(self):
        blob, _ = self.fixture()
        original = bytes(blob)
        clone = original[:8] + NEW_SHA1.encode('ascii') + original[48:]
        self.assertNotEqual(clone, original)
        self.assertEqual(clone[:8], original[:8])
        self.assertEqual(clone[48:], original[48:])
        self.assertEqual(read_snapshot(clone, NEW_SHA1), read_snapshot(original, OLD_SHA1))
        self.assertEqual(bytes(blob), original)
        with self.assertRaises(RuntimeError):
            read_snapshot(clone, OLD_SHA1)

    def test_rejects_wrong_magic_and_short_headers(self):
        blob, _ = self.fixture()
        bad_magic = bytearray(blob)
        bad_magic[:4] = b'BAD!'
        for bad in (bad_magic, b'', blob[:4], blob[:8], blob[:48], blob[:51]):
            with self.subTest(length=len(bad)), self.assertRaises(RuntimeError):
                read_snapshot(bad, OLD_SHA1)

    def test_rejects_unreviewed_versions_including_legacy(self):
        for version in (0, 1, 3, 0xffffffff):
            blob, _ = self.fixture()
            struct.pack_into('<I', blob, 4, version)
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                read_snapshot(blob, OLD_SHA1)

    def test_rejects_wrong_rom_identity(self):
        blob, _ = self.fixture()
        for identity in (NEW_SHA1, '0' * 40, '', OLD_SHA1.upper()):
            with self.subTest(identity=identity), self.assertRaises(RuntimeError):
                read_snapshot(blob, identity)
        blob[8] = 0
        with self.assertRaises(RuntimeError):
            read_snapshot(blob, OLD_SHA1)

    def test_rejects_wrong_section_counts(self):
        for count in (0, 6, 8, 0xffffffff):
            blob, _ = self.fixture()
            struct.pack_into('<I', blob, 48, count)
            with self.subTest(count=count), self.assertRaises(RuntimeError):
                read_snapshot(blob, OLD_SHA1)

    def test_rejects_duplicate_or_unknown_section_tags(self):
        for tag in (b'CPU0', b'XXXX', b'MODS'):
            blob, offsets = self.fixture()
            blob[offsets[b'META']:offsets[b'META'] + 4] = tag
            with self.subTest(tag=tag), self.assertRaises(RuntimeError):
                read_snapshot(blob, OLD_SHA1)

    def test_rejects_truncated_headers_payloads_and_oversized_lengths(self):
        blob, offsets = self.fixture()
        cuts = {52, len(blob) - 1}
        for offset in offsets.values():
            cuts.update((offset, offset + 3, offset + 7, offset + 8))
        for end in sorted(cuts):
            with self.subTest(end=end), self.assertRaises(RuntimeError):
                read_snapshot(blob[:end], OLD_SHA1)
        for tag in REQUIRED_SECTIONS:
            bad = bytearray(blob)
            struct.pack_into('<I', bad, offsets[tag] + 4, 0xffffffff)
            with self.subTest(oversized=tag), self.assertRaises(RuntimeError):
                read_snapshot(bad, OLD_SHA1)

    def test_rejects_unframed_trailing_data(self):
        blob, _ = self.fixture()
        for tail in (b'\x00', b'unframed', b'MODS' + struct.pack('<I', 0)):
            with self.subTest(tail=tail), self.assertRaises(RuntimeError):
                read_snapshot(blob + tail, OLD_SHA1)

    def test_rejects_structurally_framed_but_short_guest_memory(self):
        for size in (0, 0x40000, 0x47fff):
            blob, _ = self.fixture(bus_size=size)
            with self.subTest(size=size), self.assertRaises(RuntimeError):
                read_snapshot(blob, OLD_SHA1)


if __name__ == '__main__':
    unittest.main()
