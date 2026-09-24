"""Paired 12:5 animation regression using TCP state, never framebuffer data."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def field_frames(directory):
    return [dict(re.findall(r'(\w+)=([^ ]+)', line))
            for line in (directory/'stderr.log').read_text().splitlines()
            if '[sc3:field-frame]' in line]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--objects', choices=('native', 'enabled'), default='enabled')
    p.add_argument('--inventory', type=Path, required=True)
    p.add_argument('--extra', type=Path, nargs='*', default=[])
    a = p.parse_args()
    out = a.output.resolve()
    assert out.is_relative_to(ROOT/'validation') and not out.exists()
    out.mkdir(parents=True)
    inventory = json.loads(a.inventory.read_text())['records']
    fields = {}
    scripts = []
    for r in inventory:
        if r.get('all_sources_authenticated'):
            if r.get('state', {}).get('control') == 'free':
                fields.setdefault(tuple(x['asset'] for x in r['layers']), r['state_file'])
            else:
                scripts.append(r['state_file'])
    cases = [(f'field-{assets[0]}', state, '1023:120,991:12,1023:6,1007:12', 'field')
             for assets, state in fields.items()]
    cases += [(f'blocked-{n}', state, '1023:10,1022:1,1023:30', 'blocked') for n, state in enumerate(scripts)]
    cases += [(f'fresh-{n}', str(state.resolve()), '1023:120,991:12,1023:12,1007:12', 'fresh')
              for n, state in enumerate(a.extra)]
    battle = ROOT/'validation/playtest-20260921-152503-440/frame-0000020742-1790018925942/state.gbas'
    cases += [('battle', str(battle), '1023:6,767:3,1023:15', 'battle'),
              ('menu', next(iter(fields.values())), '1023:6,1015:1,1023:40,1021:1,1023:40', 'menu')]
    summary = []
    for name, state, sequence, kind in cases:
        for version, exe in [('before', a.before.resolve()), ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
                '--state', state, '--output', str(out/(name+'-'+version)), '--exe', str(exe),
                '--sequence', sequence, '--compact', '--objects', a.objects, '--field-audit',
                '--tool-audit', '--general-fields'], check=True, creationflags=subprocess.CREATE_NO_WINDOW)
        old, new = out/(name+'-before'), out/(name+'-after')
        x, y = [json.loads((d/'frames.json').read_text()) for d in (old, new)]
        assert len(x) == len(y) > 1
        for before, after in zip(x, y):
            assert before == after, (name, after['index'], 'guest metadata changed')
        f, g = field_frames(old), field_frames(new)
        assert len(f) == len(g) and f
        assert all(before['completed'] == after['completed'] and before['wide'] == after['wide']
                   and before['valid'] == after['valid'] for before, after in zip(f, g)), (name, 'framing changed')
        if kind in ('field', 'fresh'):
            assert any(frame['wide'] == '1' for frame in g), (name, 'never widened')
        if kind == 'battle':
            assert all(frame['wide'] == '0' for frame in g)
        for frame in g:
            if frame['wide'] == '1':
                assert int(frame['animations']) > 0, (name, 'animation resource path not used')
        summary.append(dict(case=name, frames=len(x), guest_state_unchanged=True,
            framing_unchanged=True, widened=sum(frame['wide'] == '1' for frame in g),
            reasons=sorted({frame['reason'] for frame in g}),
            animation_decodes=max(int(frame['animations']) for frame in g)))
        report = dict(objects=a.objects, before_sha256=hashlib.sha256(a.before.read_bytes()).hexdigest(),
            after_sha256=hashlib.sha256((ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest(),
            cases=summary, frames=sum(case['frames'] for case in summary))
        (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(json.dumps(summary[-1]), flush=True)
    print('PASS paired animation regression', sum(case['frames'] for case in summary), 'frames', flush=True)


if __name__ == '__main__':
    main()
