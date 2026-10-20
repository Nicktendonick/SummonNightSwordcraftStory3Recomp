"""Audit private English 1.1.1f inputs before enabling exact-revision hooks.

No ROM or save is modified. The JSON report contains hashes/offsets, not game
bytes. This bounded compatibility audit is not a full-game correctness claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import tomllib
import zlib

ROOT = Path(__file__).resolve().parents[1]
JP_SHA1 = '3f5253fcf57e07ce52472bd29a61d16b98a12376'
OLD_SHA1 = '99ca58319b050c238b1cb61cbc6db7b418b5472c'
NEW_SHA1 = '65885d763f77648ac733daac69de15bc1ccb0ea5'
PATCH_SHA256 = '5e0693b1899306ab8f61157efc6d9cc98b4e6a7c5a7ffb954470c6040405ac4d'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def identity(data):
    return dict(bytes=len(data), sha1=hashlib.sha1(data).hexdigest(),
                sha256=sha256(data), crc32=f'{zlib.crc32(data):08X}')


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def fnv(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result


def reviewed_ranges(old, new):
    spans = [
        ('script operators', 0x1262c, 0x12700),
        ('script variable lookup', 0x12f60, 0x12fb8),
        ('arena loader', 0x264cc, 0x26a44),
        ('battle lifecycle', 0x2b95c, 0x2bc90),
        ('battle map/camera', 0x31420, 0x31bc8),
        ('raster scheduler', 0x352f4, 0x35d00),
        ('arena descriptors', 0xb801cc, 0xb80418),
        ('archive reader', 0x1d3c, 0x1e20),
        ('field loader', 0x4eb8, 0x62c0),
        ('field owner', 0x19688, 0x198fc),
        ('field actions before expiry literal', 0x93994, 0xa4560),
        ('field actions after expiry literal', 0xa4564, 0xa6a4c),
    ]
    config = tomllib.loads((ROOT / 'symbols/swordcraft3_jp.toml').read_text())
    for item in config['code_copy']:
        start = item['source_start'] - 0x08000000
        spans.append((item['name'], start, start + item['size']))
    for item in config['jump_table']:
        start = item['addr'] - 0x08000000
        spans.append((item['name'], start, start + item['stride'] * item['count']))
    for filename in ('guard_experiment.h', 'custom_battle_spell_window.h'):
        parsed = re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)ull\s*\}',
                            (ROOT / 'src' / filename).read_text())
        require(parsed, 'No hook signatures found: ' + filename)
        for address, length, expected in parsed:
            address, length = int(address, 16), int(length, 16)
            require(fnv(new[address:address+length]) == int(expected, 16),
                    'Hook signature changed: ' + filename)
            spans.append((filename, address, address + length))
    for address in (0x9b9e, 0x9bb4, 0x91ce, 0x91e0):
        spans.append(('OBJ submission context', address-8, address+10))
    rows = []
    for name, start, end in spans:
        require(0 <= start < end <= len(new), 'Invalid audit span')
        require(old[start:end] == new[start:end], 'Changed reviewed span: ' + name)
        rows.append(dict(name=name, start=hex(start), end_exclusive=hex(end),
                         sha256=sha256(new[start:end]), identical=True))
    # The existing field audit identified this as a data pointer loaded by
    # 080A4538 and passed to the message routine, not an instruction or callback.
    require(u32(old, 0xa4560) == 0x08004194, 'Old expiry literal changed')
    require(u32(new, 0xa4560) == 0x0800b140, 'New expiry literal changed')
    require(old[0x4194:0x41a9] == new[0xb140:0xb155] == b"\x83\xc3's effect expired\x00\x00",
            'Expiry message relocation changed content')
    return rows


def arena_maps(old, new):
    """Authenticate unchanged archive directory, compressed maps and metadata."""
    def member(base, index):
        entry = base + 8 + index * 8
        require(0 <= entry <= len(new)-8, 'Archive entry outside ROM')
        require(old[entry:entry+8] == new[entry:entry+8], 'Archive directory changed')
        pointer = base + 16 * u32(new, entry)
        require(0 <= pointer < len(new), 'Archive member outside ROM')
        return pointer

    def decompress(offset):
        require(new[offset] == 0x10, 'Unknown map compression')
        size = int.from_bytes(new[offset+1:offset+4], 'little')
        require(0 < size <= 0x40000, 'Unexpected map size')
        cursor, output = offset + 4, bytearray()
        while len(output) < size:
            require(cursor < len(new), 'Truncated compressed map')
            flags = new[cursor]
            cursor += 1
            for bit in range(7, -1, -1):
                if len(output) == size:
                    break
                if flags & (1 << bit):
                    require(cursor+2 <= len(new), 'Truncated map reference')
                    a, b = new[cursor:cursor+2]
                    cursor += 2
                    length, distance = (a >> 4) + 3, ((a & 15) << 8) + b + 1
                    require(distance <= len(output) and len(output)+length <= size,
                            'Invalid map reference')
                    for _ in range(length):
                        output.append(output[-distance])
                else:
                    require(cursor < len(new), 'Truncated map literal')
                    output.append(new[cursor])
                    cursor += 1
        require(old[offset:cursor] == new[offset:cursor], 'Compressed arena map changed')
        return cursor, bytes(output)

    archive, rows = member(0xbda40c, 6), []
    for arena in range(21):
        descriptor = new[0xb801cc+arena*28:0xb801cc+(arena+1)*28]
        for role, index in (('near', descriptor[1]), ('far', descriptor[17])):
            if index == 255:
                continue
            start = member(archive, index)
            end, data = decompress(start)
            rows.append(dict(arena=arena, role=role, asset=index, start=hex(start),
                             end_exclusive=hex(end), compressed_sha256=sha256(new[start:end]),
                             decoded_map_sha256=sha256(data), decoded_size=len(data),
                             metadata=list(struct.unpack_from('<6HI', data, 16)), identical=True))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('japanese', 'previous', 'english', 'patch', 'output'):
        parser.add_argument('--' + flag, type=Path, required=True)
    args = parser.parse_args()
    inputs = [args.japanese, args.previous, args.english, args.patch]
    blobs = [path.read_bytes() for path in inputs]
    jp, old, new, patch = blobs
    for label, rom, expected in [('Japanese', jp, JP_SHA1), ('English 1.1.0f', old, OLD_SHA1),
                                 ('English 1.1.1f', new, NEW_SHA1)]:
        require(len(rom) == 0x2000000 and hashlib.sha1(rom).hexdigest() == expected,
                'Unexpected input identity: ' + label)
    require(sha256(patch) == PATCH_SHA256 and patch[:4] == b'BPS1', 'Unexpected patch')
    require(struct.unpack('<III', patch[-12:]) ==
            (zlib.crc32(jp), zlib.crc32(new), zlib.crc32(patch[:-4])), 'BPS checksum failure')
    ranges, maps = reviewed_ranges(old, new), arena_maps(old, new)
    report = dict(passed=True, japanese=identity(jp), previous=identity(old), english=identity(new),
                  patch=identity(patch), bps_checksums_valid=True,
                  changed_bytes=sum(a != b for a, b in zip(old, new)), ranges=ranges,
                  reviewed_expiry_relocation=dict(offset='0xa4560', old='0x08004194', new='0x0800b140'),
                  arena_maps=maps,
                  scope='Bounded hook/code-copy/map audit; regenerate translated code and test gameplay separately')
    for path, blob in zip(inputs, blobs):
        require(sha256(path.read_bytes()) == sha256(blob), 'Input changed during audit')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(f'PASS: {len(ranges)} reviewed spans; {len(maps)} arena map roles; exact input checksums')


if __name__ == '__main__':
    main()
