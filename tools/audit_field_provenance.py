"""Audit field task ownership and ROM-backed map provenance, without rendering.

This is a diagnostic, not permission to widen an unreviewed scene. See
docs/FIELD_PROVENANCE_20260922.md. All pointer reads are bounded; unknown data
fails closed. Reports contain metadata only, not ROM bytes or tile images.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from inspect_scene_state import memory

ROOT = Path(__file__).resolve().parents[1]
ROM_HASHES = {
    '39bc4cf448106aa4b8cdde235632ffb57432c4b1919c8843510b70b3787fad2d': 'jp',
    'dbc6934925de2df75f94814f810c598519452b6aec75450b597e082adaaae735': 'beta',
}


def span(data, offset, size):
    if offset < 0 or size < 0 or offset > len(data) or size > len(data)-offset:
        raise ValueError('out-of-range span')
    return data[offset:offset+size]


def u16(data, offset):
    return struct.unpack('<H', span(data, offset, 2))[0]


def u32(data, offset):
    return struct.unpack('<I', span(data, offset, 4))[0]


def scheduler(e, i):
    # 0801978C initializes this exact 16-record pool. 08019688 walks its
    # linked list, calling +8 only when (flags & 0x8800) == 0x8000.
    if u32(i, 0x699c) != 0x02000800:
        raise ValueError('unrecognized task pool')
    current = u32(e, 0xa00)
    previous = 0
    seen = set()
    tasks = []
    while current:
        offset = current-0x02000800
        if offset < 0 or offset >= 16*32 or offset % 32 or current in seen:
            raise ValueError('invalid or cyclic task link')
        seen.add(current)
        pos = current-0x02000000
        if u32(e, pos+0x18) != previous:
            raise ValueError('inconsistent task backlink')
        flags = u16(e, pos)
        callback = u32(e, pos+8)
        pending = u32(e, pos+0xc)
        tasks.append(dict(address=hex(current), flags=flags, callback=hex(callback),
                          scheduled=(flags & 0x8800) == 0x8000,
                          retiring=bool(flags & 0x80),
                          replacing=bool(flags & 0x40), pending_callback=hex(pending)))
        previous, current = current, u32(e, pos+0x1c)
    if len(tasks) != u16(e, 0xa04):
        raise ValueError('task count disagrees with linked list')
    return tasks


def field_state(e, i):
    tasks = scheduler(e, i)
    owners = [t for t in tasks if t['callback'] == '0x8093995' and
              t['scheduled'] and not t['retiring'] and not t['replacing']]
    result = dict(tasks=tasks, owner_count=len(owners), owned=False,
                  control='not-field-owned')
    if len(owners) != 1:
        return result
    pointer = u32(i, 0x6b54)
    if pointer & 3 or not 0x02000000 <= pointer <= 0x02040000-0x564:
        raise ValueError('invalid field structure')
    flags = u16(e, pointer-0x02000000)
    result.update(owned=True, pointer=hex(pointer), flags=flags,
                  control='free' if flags & 0x1005 == 1 else 'scripted-or-blocked')
    return result


def lz77(rom, offset):
    header = span(rom, offset, 4)
    if header[0] != 0x10:
        raise ValueError('unsupported compression')
    size = int.from_bytes(header[1:4], 'little')
    if size < 32 or size > 0x40000:
        raise ValueError('invalid expanded map length')
    pos = offset+4
    out = bytearray()
    while len(out) < size:
        flags = span(rom, pos, 1)[0]
        pos += 1
        for bit in range(7, -1, -1):
            if len(out) == size:
                break
            if flags & (1 << bit):
                a, b = span(rom, pos, 2)
                pos += 2
                count, distance = (a >> 4)+3, ((a & 15) << 8)+b+1
                if distance > len(out) or count > size-len(out):
                    raise ValueError('invalid compressed map reference')
                for _ in range(count):
                    out.append(out[-distance])
            else:
                out.extend(span(rom, pos, 1))
                pos += 1
    return bytes(out)


def archive_member(rom, base, index):
    # 08001D3C / 08001D78 use eight-byte entries and 16-byte offset units.
    # The second entry word is not used as a member-size assertion.
    if base < 0 or base % 4 or not 0 <= index < 0xffff:
        raise ValueError('invalid archive selection')
    relative = u32(rom, base+8+index*8)*16
    if relative < 8+(index+1)*8:
        raise ValueError('archive member points into its table')
    target = base+relative
    span(rom, target, 4)
    return target


def inspect_layers(e, i, rom, state):
    f = int(state['pointer'], 16)-0x02000000
    # Archive table itself is in IWRAM, not ROM. 08094A4C selects (1,2).
    root = u32(i, 0x2974)-0x08000000
    archive = archive_member(rom, root, 2)
    layers = []
    for bg in range(1, 4):
        result = dict(bg=bg, authenticated=False)
        try:
            record = f+0x4e0+(bg-1)*0x2c
            asset = u16(e, record+0x1c)
            result.update(asset=asset, field_flags=u16(e, record+0x18))
            if asset == 0xffff:
                result['reason'] = 'absent-layer'
                layers.append(result)
                continue
            resource = archive_member(rom, archive, asset)
            data = lz77(rom, resource)
            fmt, reserved, width, height, _, _ = struct.unpack('<6H', span(data, 16, 12))
            start = u32(data, 28) & 0xfffffffc
            result.update(resource=hex(resource+0x08000000), format=fmt,
                          width=width, height=height)
            if fmt != 0x4000 or reserved != 0 or not width or not height or width % 8 or height % 8:
                raise ValueError('unreviewed map format')
            size = (width//8)*(height//8)*2
            authored = span(data, start, size)
            d = 0x2a20+bg*0x34
            ptr = u32(i, d+0x1c)
            live = span(e, ptr-0x02000000, size)
            if u16(i,d+4) != width or u16(i,d+6) != height or u16(i,d) & 0x4000 == 0:
                raise ValueError('loaded descriptor/header disagreement')
            if authored != live:
                raise ValueError('authored/live source disagreement')
            result.update(authenticated=True, source=hex(ptr), source_sha1=hashlib.sha1(live).hexdigest(),
                          animations=u16(i,d+0x10), scroll=[u16(i,d+8),u16(i,d+10)],
                          reason='rom-source-match')
        except (ValueError, struct.error) as error:
            result['reason'] = str(error)
        layers.append(result)
    return layers


def inspect(e, i, rom):
    result = dict(ownership_valid=False, render_authorized=False)
    try:
        state = field_state(e, i)
        result.update(state=state, ownership_valid=True)
        if state['owned']:
            result['layers'] = inspect_layers(e, i, rom, state)
            result['all_sources_authenticated'] = all(x['authenticated'] for x in result['layers'])
            result['scripted_animation_active'] = any(i[0x29c0+n*8] for n in range(12))
    except (ValueError, struct.error) as error:
        result['reason'] = str(error)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('states', type=Path, nargs='+', help='GBAS files or capture directories (recursive)')
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(ROOT/'validation'):
        p.error('Reports must remain in this worktree validation directory')
    if out.exists():
        p.error('Refusing to overwrite an existing report')
    rom = a.rom.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest not in ROM_HASHES:
        p.error('Unreviewed ROM revision')
    states = sorted({f.resolve() for path in a.states for f in
                     (path.rglob('*.gbas') if path.is_dir() else [path])})
    if not states:
        p.error('No saved states found')
    records = []
    for path in states:
        try:
            e, i = memory(path)
            row = inspect(e, i, rom)
        except (ValueError, struct.error) as error:
            row = dict(ownership_valid=False, reason=str(error), render_authorized=False)
        row['state_file'] = str(path)
        row['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        records.append(row)
    counts = Counter()
    for row in records:
        counts['captures'] += 1
        counts['field_owned'] += int(row.get('state',{}).get('owned',False))
        counts['free_control'] += int(row.get('state',{}).get('control') == 'free')
        counts['all_sources_authenticated'] += int(row.get('all_sources_authenticated',False))
        counts['invalid_ownership'] += int(not row['ownership_valid'])
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(dict(rom_sha256=digest,edition=ROM_HASHES[digest],
        summary=dict(counts),records=records),indent=2),encoding='utf-8')
    print(json.dumps(dict(counts)))


if __name__ == '__main__':
    main()
