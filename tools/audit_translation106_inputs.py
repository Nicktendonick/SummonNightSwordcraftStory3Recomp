"""Private 1.0.6.f input audit and narrowly scoped diagnostic fixtures.

This is NOT a player save-state converter. It never modifies source inputs or
runtime authentication. Only ten known arena test fixtures are cloned after
exact ROM identities, the one-byte ROM delta, all generated code, reviewed
code/data spans and arena map inputs have passed. Clones differ ONLY in the
GBAS header's 40 ASCII ROM-identity bytes. Gameplay testing remains necessary.
No pixels or image assertions are used. Reports/fixtures stay under validation/.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import time
import tomllib
import zlib

from validate_battle_camera import FIXTURES
from validate_translation105 import ROOT, OWNER, ROM as OLD_ROM, JP

NEW_ROM = ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
PATCH = OWNER.parent / 'Swordcraft Story 3 rom files/Hajimari_no_Ishi_v1.0.6.f.bps'
OLD_SHA1 = '06a9f4db52f40a7034dc1c74161a705f30edb858'
NEW_SHA1 = '6753a22a096b8adaa3a869333b99fcfe29ba1fec'
REQUIRED_SECTIONS = {b'CPU0', b'BUS0', b'IO_0', b'AUD0', b'SAV0', b'PPU0', b'META'}
ARENAS = (0, 1, 2, 3, 7)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(blob):
    return hashlib.sha256(blob).hexdigest()


def identity(blob):
    return dict(size=len(blob), sha1=hashlib.sha1(blob).hexdigest(),
                sha256=sha256(blob), crc32=f'{zlib.crc32(blob):08X}')


def u32(blob, offset):
    return struct.unpack_from('<I', blob, offset)[0]


def read_snapshot(blob, expected_sha1):
    """Mirror snapshot.cpp framing checks, without relaxing its ROM gate."""
    require(len(blob) >= 52 and blob[:4] == b'GBAS', 'Invalid GBAS header')
    require(u32(blob, 4) == 2, 'Only the reviewed version 2 fixtures are allowed')
    require(blob[8:48] == expected_sha1.encode('ascii'), 'Fixture ROM identity mismatch')
    require(u32(blob, 48) == 7, 'Expected the unmodified seven-section fixture catalog')
    cursor, result = 52, {}
    for _ in range(u32(blob, 48)):
        require(cursor + 8 <= len(blob), 'Truncated section header')
        tag, length = struct.unpack_from('<4sI', blob, cursor)
        cursor += 8
        require(tag not in result and length <= len(blob) - cursor, 'Invalid section span')
        result[tag] = dict(offset=cursor, size=length, sha256=sha256(blob[cursor:cursor+length]))
        cursor += length
    require(cursor == len(blob), 'Unexpected trailing snapshot bytes')
    require(set(result) == REQUIRED_SECTIONS, 'Unexpected fixture section catalog')
    require(result[b'BUS0']['size'] >= 0x48000, 'Truncated guest memory')
    return result


def generated_audit():
    old_dir, new_dir = (ROOT / f'build-native/generated-release{version}' for version in (105, 106))
    expected = {f'recompiled_{n:03d}.cpp' for n in range(32)}
    expected.update(('recompiled.h', 'dispatch_table.cpp', 'symbol_map.cpp'))
    require({p.name for p in old_dir.iterdir() if p.is_file()} == expected, 'Unexpected 105 generated file set')
    require({p.name for p in new_dir.iterdir() if p.is_file()} == expected, 'Unexpected 106 generated file set')
    rows, totals = [], {'definitions': 0, 'declarations': 0, 'dispatch': 0, 'symbols': 0}
    for name in sorted(expected):
        old, new = ((folder / name).read_bytes() for folder in (old_dir, new_dir))
        normalized = new
        replacements = 0
        for index in range(16):
            before = f'release105_script_binary_operators_{index:02d}'.encode()
            after = f'release106_script_binary_operators_{index:02d}'.encode()
            require(old.count(before) == new.count(after), f'Changed operator references: {name}')
            require(after not in old and before not in new, f'Mixed revision names: {name}')
            replacements += new.count(after)
            normalized = normalized.replace(after, before)
        require(old == normalized, f'Unreviewed generated-code change: {name}')
        if name.startswith('recompiled_'):
            totals['definitions'] += replacements
        elif name == 'recompiled.h':
            totals['declarations'] += replacements
        elif name == 'dispatch_table.cpp':
            totals['dispatch'] += replacements
        elif name == 'symbol_map.cpp':
            totals['symbols'] += replacements
        rows.append(dict(file=name, old_sha256=sha256(old), new_sha256=sha256(new),
                         byte_identical=old == new, allowed_identifier_renamings=replacements,
                         equivalent_after_exact_16_name_mapping=True))
    require(totals == dict(definitions=16, declarations=16, dispatch=16, symbols=16),
            'Unexpected metadata operator symbol counts')
    return dict(passed=True, files=rows, totals=totals,
                comparison='Exact bytes except the 16 enumerated metadata-generated operator names; no whitespace or comments discarded')


def range_audit(old, new):
    ranges = [
        ('script operator instructions/table', 0x1262c, 0x12700),
        ('script variable lookup', 0x12f60, 0x12fb8),
        ('arena loader', 0x264cc, 0x26a44),
        ('battle lifecycle', 0x2b95c, 0x2bc90),
        ('battle map loader and camera', 0x31420, 0x31bc8),
        ('normal raster scheduler', 0x352f4, 0x35d00),
        ('arena descriptors', 0xb801cc, 0xb80418),
        ('archive routines', 0x1d3c, 0x1e20),
        ('field loader', 0x4eb8, 0x62c0),
        ('field owner', 0x19688, 0x198fc),
        ('field actions/residency/draw/shadows', 0x93994, 0xa6a4c),
    ]
    config = tomllib.loads((ROOT / 'symbols/swordcraft3_jp.toml').read_text())
    for item in config['code_copy']:
        start = item['source_start'] - 0x08000000
        ranges.append((item['name'], start, start + item['size']))
    for item in config['jump_table']:
        start = item['addr'] - 0x08000000
        ranges.append((item['name'], start, start + item['stride'] * item['count']))
    for name in ('guard_experiment.h', 'custom_battle_spell_window.h'):
        source = (ROOT / 'src' / name).read_text()
        parsed = re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)ull\s*\}', source)
        require(parsed, f'No reviewed guard spans in {name}')
        for start, length, _ in parsed:
            start, length = int(start, 16), int(length, 16)
            ranges.append((name, start, start + length))
    for address in (0x9b9e, 0x9bb4, 0x91ce, 0x91e0):
        ranges.append(('OBJ submission guard', address - 8, address + 10))
    result = []
    for name, start, end in ranges:
        require(0 <= start < end <= len(old), f'Invalid audit span: {name}')
        require(old[start:end] == new[start:end], f'Changed reviewed span: {name}')
        result.append(dict(name=name, start=hex(start), end_exclusive=hex(end),
                           sha256=sha256(new[start:end]), identical=True))
    return result


def map_audit(old, new):
    """Compare compressed spans and decoded tile-map bytes; never render pixels."""
    rows = []
    def member(base, index):
        pointer = base + 16 * u32(new, base + 8 + index * 8)
        require(pointer < len(new), 'Archive pointer outside ROM')
        require(old[base+8+index*8:base+16+index*8] == new[base+8+index*8:base+16+index*8],
                'Changed archive directory entry')
        return pointer
    def decompress(offset):
        require(new[offset] == 0x10, 'Unexpected map compression')
        size = int.from_bytes(new[offset+1:offset+4], 'little')
        require(0 < size <= 0x40000, 'Unexpected map size')
        cursor, output = offset + 4, bytearray()
        while len(output) < size:
            flags = new[cursor]
            cursor += 1
            for bit in range(7, -1, -1):
                if len(output) == size:
                    break
                if flags & (1 << bit):
                    a, b = new[cursor:cursor+2]
                    cursor += 2
                    length, distance = (a >> 4) + 3, ((a & 15) << 8) + b + 1
                    require(distance <= len(output) and len(output)+length <= size, 'Invalid map reference')
                    for _ in range(length):
                        output.append(output[-distance])
                else:
                    output.append(new[cursor])
                    cursor += 1
        require(old[offset:cursor] == new[offset:cursor], 'Changed compressed arena map')
        return cursor, bytes(output)
    archive = member(0xbda40c, 6)
    for arena in range(21):
        descriptor = new[0xb801cc+arena*28:0xb801cc+(arena+1)*28]
        for role, index in (('near', descriptor[1]), ('far', descriptor[17])):
            if index == 255:
                continue
            pointer = member(archive, index)
            end, data = decompress(pointer)
            rows.append(dict(arena=arena, role=role, asset=index, start=hex(pointer),
                             end_exclusive=hex(end), compressed_sha256=sha256(new[pointer:end]),
                             decoded_map_sha256=sha256(data), decoded_size=len(data),
                             metadata=list(struct.unpack_from('<6HI', data, 16)), identical=True))
    return rows


def main():
    blobs = {path: path.read_bytes() for path in (OLD_ROM, NEW_ROM, JP, PATCH)}
    old, new, japanese, patch = (blobs[path] for path in (OLD_ROM, NEW_ROM, JP, PATCH))
    require(identity(old)['sha1'] == OLD_SHA1 and len(old) == 0x2000000, 'Wrong 105 input')
    require(identity(new)['sha1'] == NEW_SHA1 and len(new) == len(old), 'Wrong 106 input')
    require(identity(japanese)['sha1'] == '3f5253fcf57e07ce52472bd29a61d16b98a12376', 'Wrong Japanese input')
    require(patch[:4] == b'BPS1', 'Not a BPS patch')
    require(u32(patch, len(patch)-12) == zlib.crc32(japanese), 'BPS source checksum mismatch')
    require(u32(patch, len(patch)-8) == zlib.crc32(new), 'BPS target checksum mismatch')
    require(u32(patch, len(patch)-4) == zlib.crc32(patch[:-4]), 'BPS patch checksum mismatch')
    differences = [index for index, (a, b) in enumerate(zip(old, new)) if a != b]
    require(differences == [0x17f32ea], 'Unreviewed ROM delta; do not transplant fixtures')
    report = dict(passed=False, diagnostic_only=True, inputs={str(p): identity(b) for p, b in blobs.items()},
                  bps_checksums_valid=True, rom_differences=[dict(start='0x17f32ea', end_exclusive='0x17f32eb',
                  old_value=old[0x17f32ea], new_value=new[0x17f32ea])],
                  generated=generated_audit(), unchanged_reviewed_ranges=range_audit(old, new),
                  unchanged_arena_maps=map_audit(old, new), fixtures=[])
    report['limits'] = [
        'The single changed byte is reported without guessing its semantic purpose.',
        'Generated guest instructions are equivalent under an exact metadata symbol mapping, not independently decompiled source.',
        'Private fixture header adaptation does not establish general player save-state compatibility.',
        'No runtime ROM checks were changed by this audit; no user save states were migrated.',
        'These fixtures require follow-up input-driven gameplay comparison and do not certify every story encounter.',
    ]
    prepared = []
    for arena in ARENAS:
        for filename in ('input.gbas', 'combat-ready.gbas'):
            source = FIXTURES / f'arena-{arena:02d}' / filename
            original = source.read_bytes()
            sections = read_snapshot(original, OLD_SHA1)
            clone = original[:8] + NEW_SHA1.encode('ascii') + original[48:]
            require(read_snapshot(clone, NEW_SHA1) == sections, 'Section payload changed during cloning')
            require(clone[:8] == original[:8] and clone[48:] == original[48:], 'Non-header fixture delta')
            prepared.append((source, original, clone, arena, filename, sections))
    # Only create private output once every static gate passed. Never overwrite.
    output = ROOT / 'validation' / f'translation106-input-audit-{time.time_ns()}'
    output.mkdir(parents=True, exist_ok=False)
    fixture_root = output / 'diagnostic-fixtures'
    for source, original, clone, arena, filename, sections in prepared:
        target = fixture_root / f'arena-{arena:02d}' / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as handle:
            handle.write(clone)
        require(target.read_bytes() == clone, 'Fixture output verification failed')
        require(source.read_bytes() == original, 'Original fixture changed')
        report['fixtures'].append(dict(source=str(source), target=str(target),
            source_sha256=sha256(original), target_sha256=sha256(clone), arena=arena,
            mutation='ASCII ROM SHA-1 header bytes [8,48) only',
            sections={tag.decode('ascii'): entry for tag, entry in sections.items()}, source_unchanged=True))
    for path, blob in blobs.items():
        require(path.read_bytes() == blob, f'Input changed during audit: {path}')
    report.update(passed=True, original_inputs_unchanged=True, fixture_root=str(fixture_root))
    target = output / 'AUDIT.json'
    with target.open('x', encoding='utf-8') as handle:
        json.dump(report, handle, indent=2)
        handle.write('\n')
    print(json.dumps(dict(passed=True, report=str(target), fixtures=str(fixture_root),
        fixture_count=len(report['fixtures']), generated_files=len(report['generated']['files']),
        rom_differences=report['rom_differences']), indent=2), flush=True)


if __name__ == '__main__':
    main()
