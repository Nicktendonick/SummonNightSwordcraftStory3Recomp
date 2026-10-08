"""Private release-translation checks: identities and guest code, never pixels.

Run from the owner checkout. No ROM bytes are included in reports. No existing
install, input, battery save or save state is modified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time
import tomllib
import zlib

ROOT = Path(__file__).resolve().parents[1]
OWNER = ROOT.parents[1]
ROM = ROOT / 'build-native/translation-1.0.5f/Swordcraft Story 3 English 1.0.5f.gba'
JP = OWNER.parent / 'Swordcraft Story 3 rom files/swordcraft3_jp.gba'
OLD = OWNER / 'build-beta/rom-patch-cache/swordcraft3_beta.gba'


def fnv(data):
    value = 14695981039346656037
    for b in data:
        value = ((value ^ b) * 1099511628211) & 0xffffffffffffffff
    return value


def audit(out):
    new, jp, old = (p.read_bytes() for p in (ROM, JP, OLD))
    assert len(new) == 0x2000000
    assert hashlib.sha1(new).hexdigest() == '06a9f4db52f40a7034dc1c74161a705f30edb858'
    assert zlib.crc32(new) == 0xcc25dcdc
    rows = []
    def check(name, start, size, expected=None):
        block = new[start:start+size]
        row = dict(name=name, offset=hex(start), size=size,
                   sha256=hashlib.sha256(block).hexdigest(),
                   same_as_japanese=block == jp[start:start+size],
                   same_as_beta=block == old[start:start+size])
        row['passed'] = fnv(block) == expected if expected is not None else row['same_as_japanese']
        rows.append(row)
    for filename in ('guard_experiment.h', 'custom_battle_spell_window.h'):
        source = (ROOT/'src'/filename).read_text()
        ranges = re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)ull\s*\}', source)
        assert ranges, filename
        for a, n, h in ranges:
            check(filename, int(a, 16), int(n, 16), int(h, 16))
    for a in (0x31bc8, 0x5e780):
        check('battle lifecycle signature', a, 8)
    config = tomllib.loads((ROOT/'symbols/swordcraft3_jp.toml').read_text())
    for copy in config['code_copy']:
        check(copy['name'], copy['source_start']-0x08000000, copy['size'])
    for a in (0x9b9e, 0x9bb4, 0x91ce, 0x91e0):
        check('culling instruction context', a-8, 18)
    for table in config['jump_table']:
        check(table['name'], table['addr']-0x08000000, table['stride']*table['count'])
    # Broader than the individual traced routines: preserves field owner,
    # loader, action, NPC residency/draw and shadow layouts as one unit.
    for start,end in ((0x1d3c,0x1e20),(0x4eb8,0x62c0),(0x19688,0x198fc),
                      (0x93994,0xa4560),(0xa4564,0xa6a4c)):
        check('field producer/consumer code',start,end-start)
    # 080A44CC's 080A4538 literal load passes this data to 080A53AC.
    # Only its expiry-message pointer moved; every instruction in the broad
    # field range is unchanged. It is not an allowed tool-action callback.
    assert int.from_bytes(new[0xa4560:0xa4564],'little') == 0x08004194
    assert int.from_bytes(jp[0xa4560:0xa4564],'little') == 0x080c0178
    assert new[0x4194:0x41a9] == b"\x83\xc3's effect expired\x00\x00"
    assert hashlib.sha256(new[0x1262c:0x12700]).hexdigest() == 'ab552abace428953c76f2a66d8a28930b576babc76db184023cffa77c569dc25'
    result = dict(rom_sha1=hashlib.sha1(new).hexdigest(), rom_crc32=f'{zlib.crc32(new):08X}',
                  passed=all(r['passed'] for r in rows), ranges=rows,
                  scope='Bounded hook/code-copy authentication, not full gameplay certification')
    result['reviewed_data_relocation'] = dict(literal='080A4560',old='080C0178',new='08004194',
        meaning='Expiry message passed to 080A53AC by 080A44CC; no changed field instructions')
    (out/'hook-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f"Hook audit: {sum(r['passed'] for r in rows)}/{len(rows)} ranges passed", flush=True)
    for row in rows:
        if not row['passed']: print(json.dumps(row),flush=True)
    assert result['passed'], 'Changed guest hook needs investigation; do not relax authentication'


def smoke(out, exe, rom, extra_cycles=0):
    from validate_guard_experiment import Session
    session = Session(out/'cold-boot', True, exe, rom)
    rows = []
    try:
        sequence = [(0x3ff,120),(0x3fe,3),(0x3ff,180),(0x3f7,3),(0x3ff,120)]
        # Diagnostic new-game menu/dialogue input, not a scripted full-game
        # route. No old save states, guest memory writes or ROM substitutions.
        for cycle in range(extra_cycles):
            sequence += [(0x3f7 if cycle in (40,100,180) else 0x3fe,2),(0x3ff,14)]
        for keys, count in sequence:
            for _ in range(count):
                row = session.step(keys)
                row.pop('player')
                rows.append(row)
        assert rows[-1]['frame'] >= 426
        assert len({json.dumps(r['hashes'],sort_keys=True) for r in rows}) > 20
        session.call('savestate_save',path=str(out/'new-revision-only.gbas'))
    finally:
        session.close()
        (out/'cold-boot-state.json').write_text(json.dumps(rows,indent=2)+'\n')
    stdout = (out/'cold-boot/stdout.log').read_text(errors='replace')
    stderr = (out/'cold-boot/stderr.log').read_text(errors='replace')
    assert session.process.returncode == 0, stderr[-3000:]
    assert 'dispatch_misses=0' in stdout, stdout[-3000:]
    assert 'field-rom-revision' not in stderr, 'Release field authentication rejected'
    print('PASS: cold boot, input-driven state progression, no dispatch misses', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--rom', type=Path, default=ROM)
    parser.add_argument('--extra-cycles', type=int, default=0)
    args = parser.parse_args()
    out = ROOT/'validation'/('translation105-'+str(time.time_ns()))
    out.mkdir(parents=True)
    print('Evidence: '+str(out), flush=True)
    audit(out)
    if args.exe:
        smoke(out, args.exe.resolve(), args.rom.resolve(), args.extra_cycles)
