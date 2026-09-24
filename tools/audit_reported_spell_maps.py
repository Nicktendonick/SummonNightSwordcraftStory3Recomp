"""Check reported spell tile entries against complete authored animation frames.

This examines map indices/flip bits, not colors, images or framebuffer output.
The capture's VRAM owns the displayed phase; the live work buffer can be ahead.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from audit_field_provenance import ROM_HASHES
from probe_battle_state import ROOT, OWNER

u16 = lambda b, p: struct.unpack_from('<H', b, p)[0]
u32 = lambda b, p: struct.unpack_from('<I', b, p)[0]


def bus_memory(path):
    raw = path.read_bytes()
    assert raw[:4] == b'GBAS' and u32(raw, 4) in (1, 2)
    pos = 52
    for _ in range(u32(raw, 48)):
        tag, size = raw[pos:pos+4], u32(raw, pos+4)
        pos += 8
        assert pos+size <= len(raw)
        if tag == b'BUS0':
            assert size >= 0x60400
            data = raw[pos:pos+size]
            return data[:0x40000], data[0x40000:0x48000], data[0x48400:0x60400]
        pos += size
    raise ValueError('No BUS0')


def audit(path, rom):
    e, i, v = bus_memory(path)
    assert u32(i, 0x6ac0) == 0x03000000 and u32(i, 0x1a90) == 1
    assert i[0x44e:0x450] == bytes([2, 1]) and u32(i, 0x1e44) == 0x02003200
    d = 0x2a88
    flags, width, height = u16(i, d), u16(i, d+4), u16(i, d+6)
    assert flags in (2, 6) and (width, height) == (512, 256)
    count, table_count, base = u16(i, d+16), u16(i, d+18), u16(i, d+20)
    assert 0 < count <= 64 and count == u16(i, d+22) and base+count <= 64
    work, dest = u32(i, d+32)-0x02000000, u32(i, d+36)-0x06000000
    assert 0 <= work <= len(e)-4096 and 0 <= dest <= len(v)-4096
    lst, table = u32(i, d+40)-0x08000000, u32(i, d+44)-0x08000000
    assert 0 <= lst <= len(rom)-count*6 and 0 <= table <= len(rom)-table_count*2
    bias = u16(i, d+26)+(i[d+25]<<12)
    placements = []
    for n in range(count):
        x, y, family_index = rom[lst+n*6:lst+n*6+3]
        assert family_index < table_count
        family = table+u16(rom, table+family_index*2)
        frames, _, w, h = rom[family:family+4]
        stride = (w*h+1)*2
        assert frames and w and h and x+w <= 64 and y+h <= 32
        assert family+4+frames*stride <= len(rom)
        matches = []
        for phase in range(frames):
            equal = True
            for yy in range(h):
                for xx in range(w):
                    tx, ty = x+xx, y+yy
                    if flags & 4:
                        tx = 63-tx
                    offset = (tx//32)*2048+((ty&31)*32+(tx&31))*2
                    entry = (u16(rom, family+4+phase*stride+2+(yy*w+xx)*2)+bias) & 65535
                    if flags & 4:
                        entry ^= 1024
                    equal &= u16(v, dest+offset) == entry
            if equal:
                matches.append(phase)
        assert matches, (path.name, n, 'no complete source animation matches captured VRAM')
        placements.append(dict(index=n, source_extent=[x, y, w, h],
            complete_matching_phases=matches, entries=w*h,
            next_counter=i[0x2d50+base+n], timer=i[0x2af0+base+n]))
    phases = set(placements[0]['complete_matching_phases'])
    for placement in placements[1:]:
        phases.intersection_update(placement['complete_matching_phases'])
    assert phases, 'placements do not share a coherent source phase'
    return dict(capture=path.parent.name, state_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        script_id=e[0x3208], canvas=[width, height], flags=flags,
        uploaded_map_matches_work_buffer=e[work:work+4096] == v[dest:dest+4096],
        coherent_source_phases=sorted(phases), placements=placements,
        limit='Full map-entry provenance, not visible-pixel or artistic correctness; no new rendering permission.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    output = p.parse_args().output.resolve()
    assert output.is_relative_to(ROOT/'validation')
    jp = (OWNER/'roms/swordcraft3_jp.gba').read_bytes()
    rom = (OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba').read_bytes()
    assert hashlib.sha256(rom).hexdigest() in ROM_HASHES
    # Selection allocator/callback and complete nonstreamed animation uploader.
    ranges = [(0x9d7b8,0x9d934),(0xa4bec,0xa4c3c),(0x4d6c,0x4eb8),
              (0x549c,0x5560),(0x59a0,0x5b5c)]
    assert all(jp[a:b] == rom[a:b] for a, b in ranges)
    captures = sorted((ROOT/'validation/playtest-20260923-170024-356').glob('frame-*/state.gbas'))
    assert len(captures) == 9
    report = [audit(path, rom) for path in captures[1:7]]
    output.write_text(json.dumps(dict(code_ranges_match_jp_beta=True, captures=report), indent=2))
    print('PASS: six full animated spell maps match source; selection/uploader routines match JP/beta')


if __name__ == '__main__':
    main()
