"""Verify the September 21 two-area experiment using guest state only."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct


def native_objects(data):
    # Attribute/rectangle comparison only: no tiles, colors or framebuffer.
    sizes = (((8,8),(16,16),(32,32),(64,64)),
             ((16,8),(32,8),(32,16),(64,32)),
             ((8,16),(8,32),(16,32),(32,64)))
    result = []
    for offset in range(0, 0x400, 8):
        a, b, c = struct.unpack_from('<HHH', data, offset)
        if a & 0x300 == 0x200 or a >> 14 == 3:
            continue
        w, h = sizes[a >> 14][b >> 14]
        if a & 0x300 == 0x300:
            w *= 2; h *= 2
        x, y = b & 511, a & 255
        if x >= 376: x -= 512
        if y >= 160: y -= 256
        if x < 240 and x+w > 0 and y < 160 and y+h > 0:
            result.append((a,b,c))
    return result


def records(folder, archived_executable=None):
    identity = json.loads((folder/'identity.json').read_text())
    for key, hash_key in [('state','state_sha256'),('executable','executable_sha256')]:
        actual = hashlib.sha256(Path(identity[key]).read_bytes()).hexdigest()
        if actual != identity[hash_key] and key == 'executable' and archived_executable:
            # The original build path is replaced by the build. Accept only
            # the preserved binary with the exact originally recorded hash.
            actual = hashlib.sha256(archived_executable.read_bytes()).hexdigest()
        assert actual == identity[hash_key], (folder,key)
    return identity, json.loads((folder/'frames.json').read_text())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--arena', type=int, choices=(2,7), default=2)
    a = p.parse_args()
    root = a.root.resolve()
    assert root.is_relative_to(Path(__file__).resolve().parents[1]/'validation')
    results = []
    for case in ('battle','field'):
        base = case if case == 'battle' else 'field-full'
        for objects, prefix in [('native',case+'-native-objects'),('enabled',base)]:
            before, after = root/(prefix+'-before'), root/(prefix+'-after')
            bi, b = records(before, root/'before.exe'); ai, c = records(after)
            assert bi['state_sha256'] == ai['state_sha256'] and bi['sequence'] == ai['sequence']
            assert bi['host_width'] == ai['host_width'] == 384 and len(b) == len(c)
            # Before this option existed, the probe always enabled objects.
            assert bi.get('objects','enabled') == ai.get('objects','enabled') == objects
            differences = Counter()
            for old, new in zip(b,c):
                index = new['index']
                assert old['frame'] == new['frame'] and old['key'] == new['key']
                assert old['battle'] == new['battle']
                for key in ('cycles','iwram','ewram','vram','pal','oam'):
                    if old['hashes'][key] != new['hashes'][key]: differences[key] += 1
                if objects == 'native':
                    assert old['hashes'] == new['hashes'], (case,index)
                    continue
                for key in ('cycles','vram','pal'):
                    assert old['hashes'][key] == new['hashes'][key], (case,index,key)
                if case == 'battle':
                    assert old['hashes']['ewram'] == new['hashes']['ewram']
                else:
                    x = (before/f'{index:04d}-ewram.bin').read_bytes()
                    y = (after/f'{index:04d}-ewram.bin').read_bytes()
                    iw = (after/f'{index:04d}-iwram.bin').read_bytes()
                    field = struct.unpack_from('<I',iw,0x6b54)[0]-0x02000000
                    # 080A0070 extends entity draw only; 0800A678 positions its
                    # record at +14, 08009714 stores submission results +24.
                    # Entity flags, world coordinates, animation and resources
                    # must remain identical. This route needs no NPC allocation.
                    for pos, (v,w) in enumerate(zip(x,y)):
                        if v == w: continue
                        rel = pos-field-0x1538
                        assert 0 <= rel < 32*0x3c and rel%0x3c in (0x28,0x29,0x2a,0x2b,0x38,0x39), (index,hex(pos))
                bo = (before/f'{index:04d}-oam.bin').read_bytes()
                ao = (after/f'{index:04d}-oam.bin').read_bytes()
                assert native_objects(bo) == native_objects(ao), (case,index,'native object attributes')
            frames = [dict(re.findall(r'(\w+)=([^ ]+)',line))
                      for line in (after/'stderr.log').read_text().splitlines() if '[sc3:state-frame]' in line]
            assert len(frames) >= len(c)-1
            # First restored partial frame may not yet have an ownership hook.
            assert all(f['wide']=='1' for f in frames[1:]), (case,'fallback after warmup')
            assert all(f['active']==('1' if case=='battle' else '0') for f in frames[1:])
            if case == 'battle':
                assert all(f['arena']==str(a.arena) for f in frames[1:])
            results.append(dict(case=case, objects=objects, samples=len(c), wide_after_warmup=len(frames)-1,
                                differences=dict(differences), native_object_attributes_unchanged=True))
    (root/'report.json').write_text(json.dumps(results,indent=2))
    print(json.dumps(results,indent=2))


if __name__ == '__main__':
    main()
