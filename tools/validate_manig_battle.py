"""Private TCP regression for the Manig rocky arena; never changes input saves."""
import argparse
import hashlib
import json
from pathlib import Path
from validate_field_objects_tcp import ROOT, OWNER, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous-executable', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--width', type=int, choices=(284, 320, 384), required=True)
    parser.add_argument('--cases', nargs='+', help='Run only selected case names')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    assert out.is_relative_to(ROOT/'validation')
    out.mkdir(exist_ok=False)
    rocky = OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000061510-1788964715395/state.gbas'
    victory = ROOT/'validation/playtest-20260913-215538-152/frame-0000037049-1789351389376/state.gbas'
    forest = ROOT/'validation/playtest-20260913-215538-152/frame-0000005817-1789350991388/state.gbas'
    critical = OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas'
    cases = [
        ('rocky-idle', rocky, [1023]*90, True),
        ('rocky-r-slots', rocky, [1023]*6+([0x2ff]*3+[1023]*15)*8, True),
        ('rocky-pause', rocky, [1023]*6+[0x3f7]*3+[1023]*60+[0x3f7]*3+[1023]*30, True),
        ('rocky-walk', rocky, [1023]*6+[0x3df]*90+[0x3ef]*120+[1023]*10, True),
        ('rocky-attack', rocky, [1023]*6+([0x3fd]*3+[1023]*15)*10, True),
        ('rocky-attack-a', rocky, [1023]*6+[0x3df]*25+([0x3fe]*3+[1023]*15)*5, True),
        ('rocky-victory', victory, [1023]*60, True),
        ('rocky-jump', rocky, [1023]*6+[0x3bf]*120+[1023]*60, True),
        ('forest-idle', forest, [1023]*60, False),
        ('forest-critical', critical, [1023]*90, False),
        ('forest-jump', forest, [1023]*6+[0x3bf]*120+[1023]*60, True),
        ('forest-pause', forest, [1023]*6+[0x3f7]*3+[1023]*60+[0x3f7]*3+[1023]*30, True),
    ]
    if args.cases:
        assert set(args.cases)<={c[0] for c in cases}
        cases=[c for c in cases if c[0] in args.cases]
    results = []
    for name, state, schedule, changes in cases:
        state_hash = hashlib.sha256(state.read_bytes()).hexdigest()
        pictures, hashes = {}, {}
        for mode, enabled, exe in [('before', 1, args.previous_executable.resolve()),
                                   ('after', 1, None), ('rollback', 0, None)]:
            pictures[mode], hashes[mode] = run(state, out/name/mode, args.width, enabled,
                len(schedule), 1023, 'rocky', executable=exe, schedule=schedule,draw_audit=True,
                screenshots=(18,26,34))
        before, after, rollback = (pictures[k] for k in ('before', 'after', 'rollback'))
        rollback_reference='after' if name in ('forest-jump','forest-pause') else 'before'
        assert pictures[rollback_reference] == rollback, f'{name}: rollback picture changed'
        assert hashes[rollback_reference] == hashes['rollback'], f'{name}: rollback state changed'
        assert all((f,n)==(g,m) for (f,n,_),(g,m,_) in zip(before,after)), f'{name}: native center changed'
        wide, changed = [], []
        left=(args.width-240)//2
        for (frame,_,a),(_,_,b) in zip(before,after):
            if a!=b: changed.append(frame)
            if any(b''.join(b[y*args.width*3:(y*args.width+left)*3]+
                           b[(y*args.width+left+240)*3:(y+1)*args.width*3] for y in range(160))):
                wide.append(frame)
        if changes:
            assert changed, f'{name}: no widened frames'
            missing=[frame for frame,_,_ in after if frame not in wide]
            assert not missing or missing==[after[0][0]], f'{name}: dropped wide frames {missing}'
        else:
            assert before==after, f'{name}: supported forest full-image regression'
            assert hashes['before']==hashes['after'], f'{name}: forest guest state changed'
        assert hashlib.sha256(state.read_bytes()).hexdigest()==state_hash
        differences={key:sum(a[key]!=b[key] for a,b in zip(hashes['before'],hashes['after']))
                     for key in ('ewram','iwram','vram','pal','oam','cycles')}
        # Source-owned draw changes: csm3 copy.c / code_copy.s / linker.ld.
        # Priority heads, OAM buffer, submission count/list and 96 sprite draw
        # records may differ; simulation RAM, resident graphics and time may not.
        def draw_byte(i):
            return (0x37a0<=i<0x37b0 or 0x38b0<=i<0x3cb0 or i==0x3cb0 or
                    (0x3cc0<=i<0x40c0 and (i-0x3cc0)%8<6) or
                    (0x4540<=i<0x4b40 and (i-0x4540)%16<10))
        iwram_differences=set()
        for a,b in zip(hashes['before'],hashes['after']):
            for i,(x,y) in enumerate(zip(a['iwram_data'],b['iwram_data'])):
                if x!=y:
                    assert draw_byte(i), f'{name}: unexplained IWRAM difference {i:#x}'
                    iwram_differences.add(i)
            assert b['iwram_data'][0x3cb0]<=128, 'OAM submission capacity exceeded'
        assert not any(differences[k] for k in ('ewram','vram','pal','cycles')), f'{name}: simulation/resource divergence {differences}'
        result=dict(name=name, frames=len(schedule), wide_frames=len(wide),
            changed_frames=len(changed), native_center_identical=True, exact_rollback=True,
            state_sha256=state_hash, state_unchanged=True, region_difference_counts=differences,
            allowed_iwram_difference_offsets=[hex(i) for i in sorted(iwram_differences)],
            last_host_rgb_sha256=hashlib.sha256(after[-1][2]).hexdigest())
        results.append(result)
        (out/'report.json').write_text(json.dumps(dict(width=args.width, cases=results,
            executable_sha256=hashlib.sha256((ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest(),
            previous_executable_sha256=hashlib.sha256(args.previous_executable.read_bytes()).hexdigest()), indent=2))
        print(f'{name}: {len(wide)}/{len(schedule)} wide frames, center/state contract and rollback PASS', flush=True)


if __name__=='__main__':
    main()
