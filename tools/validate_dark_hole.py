"""Isolated 12:5 Dark Hole lifecycle/regression checks; no image assertions."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from probe_battle_state import ROOT
from validate_spell_windows import traces

CAPTURE = ROOT/'validation/playtest-20260922-171531-043'
CASTING = ROOT/'validation/playtest-20260922-164002-290'
CASES = [
    ('precast', CAPTURE/'frame-0000062376-1790111810255/state.gbas',
     '1023:26,767:8,1023:21,767:9,1023:55,1021:15,1023:240', 19),
    ('reported', CAPTURE/'frame-0000062549-1790111822774/state.gbas', '1023:180', 7),
    ('repeat2', CAPTURE/'frame-0000062549-1790111822774/state.gbas', '1023:60', 7),
    ('repeat3', CAPTURE/'frame-0000062549-1790111822774/state.gbas', '1023:60', 7),
    ('casting', CASTING/'frame-0000061488-1790109682039/state.gbas', '1023:180', 0),
    ('casting-cleanup', CASTING/'frame-0000061517-1790109680143/state.gbas', '1023:120', 0),
    ('aqua-edge', ROOT/'validation/playtest-20260922-160731-035/frame-0000015765-1790107822613/state.gbas',
     '1023:120', 0),
]


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def read(p):
    return json.loads(p.read_text())


def run_or_verify(state, exe, dest, sequence):
    if not dest.exists():
        subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
            '--state', str(state), '--exe', str(exe), '--output', str(dest),
            '--sequence', sequence, '--compact', '--draw-audit',
            '--general-fields', '--window-audit'], check=True,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    # Only complete, identity-matching runs may be reused. Never overwrite a
    # partial run or silently accept evidence from another binary or input.
    identity = read(dest/'identity.json')
    assert identity['state_sha256'] == digest(state)
    assert identity['executable_sha256'] == digest(exe)
    assert Path(identity['state']).resolve() == state.resolve()
    assert Path(identity['executable']).resolve() == exe.resolve()
    assert identity['sequence'] == sequence and identity['host_width'] == 384
    assert identity['window_audit'] and identity['general_fields']
    assert identity['objects'] == 'enabled'
    assert not identity['field_audit'] and not identity['tool_audit']
    return identity


def compare(before, after, expected_fallback, count):
    a, b = read(before/'frames.json'), read(after/'frames.json')
    assert len(a) == len(b) == count
    assert a == b, 'guest memory/resource hashes, cycles, input or battle state changed'
    old_rows, old_states = traces(before)
    rows, states = traces(after)
    assert list(states) == list(old_states) == list(range(1, count))
    assert rows.keys() == old_rows.keys()
    assert set(rows) == {(f, y) for f, s in states.items() if s['active'] == '1' for y in range(160)}
    recovered, small_active, small_disabled = set(), set(), set()
    for frame, s in states.items():
        old = old_states[frame]
        assert {k: v for k, v in s.items() if k not in ('wide', 'reason', 'top')} == {
            k: v for k, v in old.items() if k not in ('wide', 'reason', 'top')}
        if s['active'] == '1':
            assert s['wide'] == '1' and s['reason'] == 'none', (frame, s)
        else:
            assert frame == 1 and s == old
        if old['wide'] == '1':
            assert s == old, 'previously supported presentation state changed'
        if old['active'] == '1' and old['wide'] == '0':
            assert old['reason'] == 'raster-layout-change'
            recovered.add(frame)
            assert any(old_rows[frame, y]['layout'] == 0 for y in range(160))
    for key, row in rows.items():
        old = old_rows[key]
        # New role-policy diagnostics are not guest registers. Their own
        # authorization assertions live in validate_effect_families.py.
        diagnostic = {'layout', 'margin_layers', 'effect_policy'}
        assert {k: v for k, v in row.items() if k not in diagnostic} == {
            k: v for k, v in old.items() if k not in diagnostic}, (key, 'captured registers/window policy changed')
        if old['layout'] != row['layout']:
            assert row['bg2'] == 0x0385 and (row['display'] & 0xe007) == 1
            assert old['layout'] == 0 and row['layout'] == 1
            (small_active if row['display'] & 0x400 else small_disabled).add(key[0])
    assert len(recovered) == expected_fallback, (len(recovered), expected_fallback)
    if expected_fallback:
        assert small_active and small_disabled, 'missing active/retirement coverage'
    for f in range(count):
        for region in ('iwram', 'oam'):
            assert (before/f'{f:04d}-{region}.bin').read_bytes() == (after/f'{f:04d}-{region}.bin').read_bytes()
    return dict(frames=count, recovered_frames=sorted(recovered),
        bounded_affine_active=sorted(small_active), bounded_affine_disabled=sorted(small_disabled),
        unchanged_guest_state=True, continuous_owned_widescreen=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(ROOT/'validation') and out != ROOT/'validation'
    out.mkdir(parents=True, exist_ok=True)
    before, after = args.before.resolve(), ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
    report = dict(before_sha256=digest(before), after_sha256=digest(after), host_width=384,
        validator_sha256=digest(Path(__file__)), cases=[], pass_complete=False)
    for name, state, sequence, expected in CASES:
        count = sum(int(part.split(':')[1]) for part in sequence.split(','))
        for label, exe in [('before', before), ('after', after)]:
            identity = run_or_verify(state, exe, out/name/label, sequence)
            assert identity['frames'] == count
        result = dict(case=name, **compare(out/name/'before', out/name/'after', expected, count))
        if name == 'precast':
            kinds = [(out/name/'after'/f'{f:04d}-iwram.bin').read_bytes()[0x44e:0x450] for f in range(count)]
            assert kinds[0] == b'\x01\x02' and kinds[-1] == b'\x01\x02'
            assert {k[1] for k in kinds if k[0] == 9} == {1, 2, 3}
            result['input_driven_full_lifecycle'] = True
        report['cases'].append(result)
        print(json.dumps(result), flush=True)
    for label in ('before', 'after'):
        original = out/'reported'/label
        rr, ss = traces(original)
        rr = {k: v for k, v in rr.items() if k[0] < 60}
        ss = {k: v for k, v in ss.items() if k < 60}
        for repeat in ('repeat2', 'repeat3'):
            assert read(out/repeat/label/'frames.json') == read(original/'frames.json')[:60]
            assert traces(out/repeat/label) == (rr, ss)
    report.update(pass_complete=True, paired_frames=sum(c['frames'] for c in report['cases']),
        independent_restorations=3, note='Memory/register/control-flow checks; no framebuffer or independent CPU oracle.')
    (out/'report.json').write_text(json.dumps(report, indent=2))
    print('PASS Dark Hole:', report['paired_frames'], 'paired frames', flush=True)


if __name__ == '__main__':
    main()
