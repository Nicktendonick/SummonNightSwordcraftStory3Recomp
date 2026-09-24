"""Paired, state-only field-tool regressions using the local TCP debugger."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CAPTURES = ROOT/'validation/playtest-20260922-130447-778'
REPEATS = '1023:6,1022:1,1023:60,1022:1,1023:60'
BOX_ROUTE = '1023:30,991:38,959:78,1023:3,1022:1,1023:60,1022:1,1023:60,1022:1,1023:60'


def field_frames(path):
    return [dict(re.findall(r'(\w+)=([^ ]+)', line))
            for line in (path/'stderr.log').read_text().splitlines() if '[sc3:field-frame]' in line]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--objects', choices=('native', 'enabled'), default='native')
    a = p.parse_args()
    output = a.output.resolve()
    assert output.is_relative_to(ROOT/'validation') and not output.exists()
    output.mkdir(parents=True)
    cases = [(s.parent.name, s, '1023:35,1022:1,1023:60') for s in sorted(CAPTURES.rglob('state.gbas'))]
    assert len(cases) == 3, 'Expected the three reported tool-action captures'
    cases += [('repeated-swings', ROOT/'validation/playtest-20260922-125214-773/frame-0000012687-1790096053114/state.gbas', REPEATS),
              ('multiple-boxes', cases[0][1], BOX_ROUTE)]
    report = []
    for name, state, sequence in cases:
        for version, exe in [('before', a.before.resolve()), ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
                '--state', str(state), '--output', str(output/(name+'-'+version)),
                '--exe', str(exe), '--sequence', sequence, '--general-fields',
                '--field-audit', '--tool-audit', '--compact', '--objects', a.objects],
                check=True, creationflags=subprocess.CREATE_NO_WINDOW)
        old, new = output/(name+'-before'), output/(name+'-after')
        x, y = [json.loads((d/'frames.json').read_text()) for d in (old, new)]
        assert len(x) == len(y) > 1
        hash_differences = []
        for f, g in zip(x, y):
            for key in ('frame', 'key', 'field', 'battle', 'field_actions'):
                assert f[key] == g[key], (name, g['index'], key)
            if f['hashes'] != g['hashes']:
                hash_differences.append(dict(index=g['index'], keys=[k for k in f['hashes'] if f['hashes'][k] != g['hashes'][k]]))
        # These particular fixtures require exact parity even with NPC hooks
        # enabled. A future case needing extra residency must declare and
        # validate its permitted resource changes, not silently drop this test.
        assert not hash_differences, (name, hash_differences[:3])
        before, after = field_frames(old), field_frames(new)
        # A saved state's raster phase determines whether its first resumed
        # frame is complete. Every later frame must produce a record.
        assert len(before) == len(after) and len(x)-1 <= len(after) <= len(x)
        assert all(f['completed'] == g['completed'] for f, g in zip(before, after))
        assert all(f['wide'] == '1' for f in after), (name, [f for f in after if f['wide'] != '1'][:5])
        changed = sum(f['wide'] != g['wide'] for f, g in zip(before, after))
        assert changed > 0, (name, 'No formerly narrow frames exercised')
        callbacks = sorted({slot['callback'] for f in y for slot in f['field_actions']['actions']})
        if name == 'multiple-boxes':
            assert '0x80a0ad1' in callbacks, 'No actual object-hit action'
            assert any(sum(s['callback'] == '0x80a0ad1' for s in f['field_actions']['actions']) >= 2 for f in y)
            for target in (7, 8):
                assert y[0]['field_actions']['objects'][target]['flags'] & 1
                assert not y[-1]['field_actions']['objects'][target]['flags'] & 1, 'Target did not break'
        report.append(dict(case=name, frames=len(x), complete_wide_frames=len(after),
                           formerly_narrow_frames_fixed=changed, callbacks=callbacks,
                           action_object_and_field_state_unchanged=True, hash_differences=hash_differences))
        (output/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(name, 'PASS', len(x), 'frames;', changed, 'formerly narrow frames;',
              len(hash_differences), 'frames with hash differences', flush=True)
    print('PASS field-tool regressions', sum(r['frames'] for r in report), 'paired frames', flush=True)


if __name__ == '__main__':
    main()
