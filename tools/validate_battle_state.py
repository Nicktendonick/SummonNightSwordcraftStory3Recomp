"""Input-driven battle hook regression: memory/control flow only, no images."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import sys
from probe_battle_state import ROOT, OWNER


def validate_results_draw_delta(before_dir, after_dir, index):
    """Source-bounded native-cull change at the rewards-panel handoff only."""
    before=(before_dir/f'{index:04d}-iwram.bin').read_bytes()
    after=(after_dir/f'{index:04d}-iwram.bin').read_bytes()
    assert len(before)==len(after)==0x8000
    changed=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b]
    assert int.from_bytes(after[0x6ab4:0x6ab8],'little')==7
    if changed:
        assert after[0xf]==2
    else:
        # Hardware OAM receives the preceding software submission buffer.
        previous=(after_dir/f'{index-1:04d}-iwram.bin').read_bytes()
        assert int.from_bytes(previous[0x6ab4:0x6ab8],'little')==7 and previous[0xf]==2
    def draw_storage(i):
        return (0x37a0<=i<0x37b0 or 0x38b0<=i<0x3cb0 or i==0x3cb0 or
                0x3cc0<=i<0x40c0 and (i-0x3cc0)%8<6 or
                0x4540<=i<0x4b40 and (i-0x4540)%16<10)
    assert all(draw_storage(i) for i in changed), 'non-draw IWRAM changed'
    assert after[0x3cb0]<=before[0x3cb0]<=128
    for start in {0x4540+((i-0x4540)//16)*16 for i in changed if 0x4540<=i<0x4b40}:
        old_x=int.from_bytes(before[start+4:start+6],'little')
        assert 240<=old_x<384
        assert after[start+4:start+8]==bytes([240,0,160,0]), 'not the native hidden position'
        assert after[start+2:start+4]==b'\0\0' and after[start+8:start+10]==b'\0\0'
    def objects(folder):
        data=(folder/f'{index:04d}-oam.bin').read_bytes()
        assert len(data)==0x400
        # Attribute tuples, not tile samples or rendered pixels.
        attrs=[struct.unpack_from('<HHH',data,i) for i in range(0,0x400,8)]
        return [a for a in attrs if a[0]&0x300!=0x200]
    old,new=objects(before_dir),objects(after_dir)
    assert new==old or new==[a for a in old if not 240<=(a[1]&511)<384], 'OAM changed beyond offscreen culling'
    return len(changed)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--cases', nargs='+')
    p.add_argument('--verify-existing', action='store_true', help='Recheck existing state evidence against the exact current executable/source hashes')
    a = p.parse_args()
    out = a.output.resolve()
    assert out.is_relative_to(ROOT/'validation')
    if a.verify_existing:
        assert out.is_dir()
    else:
        out.mkdir(exist_ok=False)
    rocky = OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000061510-1788964715395/state.gbas'
    forest = ROOT/'validation/playtest-20260913-215538-152/frame-0000005817-1789350991388/state.gbas'
    critical = OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas'
    victory = ROOT/'validation/playtest-20260913-215538-152/frame-0000037049-1789351389376/state.gbas'
    field = OWNER/'validation/visible-debugger/20260827-130539-beta/captures/frame-0000019663-1787850529742/state.gbas'
    cases = [
        ('rocky-r', rocky, '1023:6,'+','.join(['767:3,1023:15']*8), True),
        ('rocky-jump', rocky, '1023:6,959:120,1023:60', True),
        ('rocky-pause', rocky, '1023:6,1015:3,1023:30,1015:3,1023:30', True),
        ('rocky-attack', rocky, '1023:6,'+','.join(['1022:3,1023:15']*8), True),
        ('forest-jump', forest, '1023:6,959:120,1023:60', True),
        ('forest-critical', critical, '1023:90', True),
        ('rocky-victory', victory, '1023:60', True),
        ('victory-exit', victory, '1023:60,1022:3,1023:30,1022:3,1023:30,1022:3,1023:30,1022:3,1023:90', None),
        ('lake-negative', field, '1023:30', False),
    ]
    if a.cases:
        assert set(a.cases)<={c[0] for c in cases}
        cases=[c for c in cases if c[0] in a.cases]
    results = []
    for name, state, sequence, battle in cases:
        runs = {}
        for label, exe in [('before', a.before.resolve()), ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            dest = out/name/label
            command=[sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
                '--state', str(state), '--output', str(dest), '--sequence', sequence,
                '--exe', str(exe), '--compact']
            if battle is None:
                command.append('--draw-audit')
            if a.verify_existing:
                identity=json.loads((dest/'identity.json').read_text())
                assert identity['executable_sha256']==hashlib.sha256(exe.read_bytes()).hexdigest()
                assert identity['state_sha256']==hashlib.sha256(state.read_bytes()).hexdigest()
                assert identity['sequence']==sequence
            else:
                subprocess.run(command, check=True)
            runs[label] = json.loads((dest/'frames.json').read_text())
        before, after = runs['before'], runs['after']
        assert len(before)==len(after)
        differences=Counter()
        audited_draw_frames=[]
        for old,new in zip(before,after):
            assert old['frame']==new['frame'] and old['key']==new['key']
            assert old['battle']==new['battle'], f'{name}: semantic battle state changed'
            for key in ('cycles','iwram','ewram','vram','pal','oam'):
                if old['hashes'][key]!=new['hashes'][key]: differences[key]+=1
            if battle is None and any(old['hashes'][k]!=new['hashes'][k] for k in ('iwram','oam')):
                count=validate_results_draw_delta(out/name/'before',out/name/'after',new['index'])
                audited_draw_frames.append(dict(index=new['index'],changed_iwram_bytes=count))
        lines=(out/name/'after/stderr.log').read_text(errors='replace').splitlines()
        frames=[dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in lines if '[sc3:state-frame]' in line]
        assert frames, f'{name}: missing state trace'
        if battle is None:
            exits=[i for i,line in enumerate(lines) if '[sc3:state-exit]' in line]
            assert len(exits)==1, f'{name}: missing result-panel event'
            after_exit=[dict(re.findall(r'(\w+)=([^ ]+)', line))
                        for line in lines[exits[0]+1:] if '[sc3:state-frame]' in line]
            assert after_exit and all(f['active']=='0' for f in after_exit)
            assert after[-1]['battle']['phase']==0, f'{name}: did not finish teardown'
            assert any(f['active']=='1' and f['wide']=='1' for f in frames)
        elif battle:
            assert all(f['active']=='1' and f['wide']=='1' for f in frames), f'{name}: {[f for f in frames if f["wide"]!="1" or f["active"]!="1"][:5]}'
        else:
            assert all(f['active']=='0' for f in frames), f'{name}: battle hook leaked into field'
        if battle is None:
            assert not set(differences)-{'iwram','oam'}, f'{name}: gameplay-state differences {differences}'
        else:
            assert not differences, f'{name}: guest-state differences {differences}'
        result=dict(case=name,frames=len(after),complete_frames=len(frames),differences=dict(differences),
            top_boundaries=sorted({int(f['top']) for f in frames}),
            camera_y=sorted({f['battle']['camera_y'] for f in after}),
            slots=sorted({f['battle']['slot'] for f in after}), audited_draw_frames=audited_draw_frames)
        results.append(result)
        (out/'report.json').write_text(json.dumps(results,indent=2))
        print(f'{name}: PASS {len(after)} frames; gameplay state identical; audited draw frames={len(audited_draw_frames)}; battle={battle}',flush=True)


if __name__=='__main__':
    main()
