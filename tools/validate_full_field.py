"""Full-frame field migration: paired state/source/ownership checks, never images."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from probe_battle_state import ROOT, OWNER
from validate_field_tools import field_frames


def cases():
    accepted = ROOT.parent/'custom-renderer'
    inventory = json.loads((accepted/'validation/field-provenance-20260922/all-captures.json').read_text())['records']
    fields, scripts = {}, []
    for record in inventory:
        if not record.get('all_sources_authenticated'):
            continue
        if record['state']['control'] == 'free':
            fields.setdefault(tuple(layer['asset'] for layer in record['layers']), Path(record['state_file']))
        else:
            scripts.append(Path(record['state_file']))
    assert len(fields) == 5 and len(scripts) == 2
    result = [(f'field-{ids[0]}', state, '1023:10,991:24,1023:6,1007:24,1023:10', 'field')
              for ids, state in fields.items()]
    result += [(f'script-{n}', state, '1023:10,1022:1,1023:30', 'script')
               for n, state in enumerate(scripts)]
    result += [('menu', next(iter(fields.values())), '1023:6,1015:1,1023:40,1021:1,1023:40', 'menu')]
    result += [('field-restore', next(iter(fields.values())), '1023:8,991:24,1023:8,1007:24,1023:8', 'restore')]
    tools = sorted((accepted/'validation/playtest-20260922-130447-778').rglob('state.gbas'))
    assert len(tools) == 3
    result += [(f'tool-{n}', state, '1023:10,1022:3,1023:50,767:3,1023:20', 'tool')
               for n, state in enumerate(tools)]
    bow = sorted((ROOT/'validation/playtest-20260923-234600-931').rglob('state.gbas'))
    assert len(bow) == 3
    result += [('bow-repeat', bow[0], '1023:45,1022:15,1023:60,1022:15,1023:45', 'tool')]
    rocky = OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000061510-1788964715395/state.gbas'
    result += [('battle-r-jump', rocky, '1023:6,767:3,1023:15,959:30,1023:20', 'battle')]
    assert all(state.is_file() for _, state, _, _ in result)
    return result


def composition(directory):
    log = (directory/'stderr.log').read_text(errors='replace')
    return [dict((k, int(v)) for k, v in re.findall(r'(\w+)=(\d+)', m))
            for m in re.findall(r'\[sc3:field-composition\] ([^\n]+)', log)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--before-full-field', action='store_true',
                   help='Engine-only comparison: preserve complete-field ownership in both binaries')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cases', nargs='+')
    p.add_argument('--verify-existing', action='store_true')
    p.add_argument('--benchmark', action='store_true')
    a = p.parse_args()
    out = a.output.resolve()
    assert out.is_relative_to(ROOT/'validation')
    selected = cases()
    if a.cases:
        assert set(a.cases) <= {c[0] for c in selected}
        selected = [c for c in selected if c[0] in a.cases]
    out.mkdir(parents=True, exist_ok=a.verify_existing)
    report = []
    for name, state, sequence, kind in selected:
        for version, exe in [('before', a.before.resolve()), ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            command = [sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'), '--state', str(state),
                       '--exe', str(exe), '--output', str(out/name/version), '--sequence', sequence,
                       '--compact', '--field-audit', '--tool-audit', '--general-fields', '--full-combat']
            if version == 'after' or a.before_full_field: command += ['--full-field']
            if kind == 'restore': command += ['--restore-at','24','48']
            if not a.verify_existing:
                subprocess.run(command, check=True, creationflags=subprocess.CREATE_NO_WINDOW)
        old, new = out/name/'before', out/name/'after'
        records = [json.loads((d/'frames.json').read_text()) for d in (old,new)]
        assert records[0] and records[0] == records[1], (name, 'guest hashes/cycles/state changed')
        identities = [json.loads((d/'identity.json').read_text()) for d in (old,new)]
        for key in ('state_sha256','sequence','host_width','objects','general_fields','full_combat'):
            assert identities[0][key] == identities[1][key], (name,key)
        assert bool(identities[0]['full_field']) == a.before_full_field and identities[1]['full_field']
        before, after = field_frames(old), field_frames(new)
        assert before == after and before, (name, 'field eligibility/animation policy changed')
        rows = composition(new)
        if a.before_full_field:
            assert composition(old) == rows, (name, 'complete-field ownership changed')
        wide = {int(row['completed']) for row in after if row['wide']=='1'}
        assert {r['completed'] for r in rows} == wide, (name, 'missing complete-field ownership')
        for row in rows:
            assert row['complete_owner']==1 and row['rows']==160 and row['center']==38400 and row['extended']==23040, (name,row)
        if kind in ('field','tool','restore'): assert rows, (name,'never rendered a full field')
        if kind in ('script','battle'): assert not rows, (name,'field claimed other owner')
        if kind == 'menu':
            assert wide and any(r['wide']=='0' for r in after), 'menu fallback not exercised'
        if kind == 'battle':
            traces = [[s for s in (d/'stderr.log').read_text().splitlines()
                       if '[sc3:composition]' in s or '[sc3:state-frame]' in s] for d in (old,new)]
            assert traces[0] == traces[1] and any('complete_owner=1' in s for s in traces[1])
        report.append(dict(case=name, frames=len(records[0]), full_field_frames=len(rows),
                           native_object_samples=sum(r['center_objects'] for r in rows),
                           extended_object_samples=sum(r['extended_objects'] for r in rows),
                           boundary_clips=sum(r['boundary_clips'] for r in rows),
                           guest_state_and_cycles_unchanged=True, identities=identities))
        (out/'report.json').write_text(json.dumps(report,indent=2))
        print(f'PASS {name}: {len(records[0])} paired frames, {len(rows)} complete field frames',flush=True)
    if a.benchmark:
        from benchmark_combat_tcp import run
        results = {}
        # Sequential identical restores; no screenshot, replay, or phase timers.
        for name,state,_,kind in selected:
            if kind != 'field': continue
            for version,exe,full in [('before',a.before.resolve(),int(a.before_full_field)),('after',ROOT/'build-native/Swordcraft3CustomRendererBeta.exe',1)]:
                r = run(state,out/f'bench-{name}-{version}',1,1,384,240,3,replay_check=0,
                        phase_profile=0,screenshots=False,general_fields=1,exe=exe,full_field=full,full_combat=1)
                results[f'{name}-{version}']=r
                print(f'{name} {version}: {r["median_ms_per_frame"]:.3f} ms/frame (headless)',flush=True)
            assert results[f'{name}-before']['end_states'] == results[f'{name}-after']['end_states']
            (out/'benchmark.json').write_text(json.dumps(results,indent=2))
    print(f'PASS {sum(r["frames"] for r in report)} paired frames',flush=True)


if __name__ == '__main__': main()
