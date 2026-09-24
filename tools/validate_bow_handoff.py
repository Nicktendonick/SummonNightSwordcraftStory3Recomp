"""Paired 12:5 bow/field regressions over isolated TCP; no image assertions."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from probe_battle_state import ROOT
from validate_field_tools import field_frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--objects', choices=('enabled', 'native'), default='enabled')
    args = parser.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(ROOT/'validation') and not out.exists()
    accepted = ROOT.parent/'custom-renderer'
    captures = sorted((ROOT/'validation/playtest-20260923-234600-931').rglob('state.gbas'))
    assert len(captures) == 3
    repeat = '1023:45,1022:15,1023:60,1022:15,1023:45'
    cases = [(f'bow-repeat-{n}', captures[0], repeat, 'bow', 3) for n in range(3)]
    cases += [(f'bow-capture-{n+2}', state, '1023:50', 'bow', 1)
              for n, state in enumerate(captures[1:])]
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
    cases += [(f'field-{ids[0]}', state, '1023:6,991:12,1023:6,1007:12', 'field', 0)
              for ids, state in fields.items()]
    cases += [(f'script-{n}', state, '1023:10,1022:1,1023:30', 'script', 0)
              for n, state in enumerate(scripts)]
    cases += [('menu', next(iter(fields.values())), '1023:6,1015:1,1023:40,1021:1,1023:40', 'menu', 0)]
    tools = sorted((accepted/'validation/playtest-20260922-130447-778').rglob('state.gbas'))
    assert len(tools) == 3
    cases += [('ordinary-tools', tools[0], '1023:35,1022:1,1023:60', 'tool', 0)]
    battle = accepted/'validation/playtest-20260921-152503-440/frame-0000020742-1790018925942/state.gbas'
    cases += [('battle-r', battle, '1023:6,767:3,1023:15', 'battle', 0)]
    assert all(state.is_file() for _, state, _, _, _ in cases)
    out.mkdir(parents=True)
    report = []
    for name, state, sequence, kind, recovered in cases:
        for version, exe in [('before', args.before.resolve()),
                             ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
                            '--state', str(state), '--exe', str(exe), '--output', str(out/name/version),
                            '--sequence', sequence, '--compact', '--field-audit', '--tool-audit',
                            '--general-fields', '--full-combat', '--objects', args.objects],
                           check=True, creationflags=subprocess.CREATE_NO_WINDOW)
        old, new = out/name/'before', out/name/'after'
        records = [json.loads((directory/'frames.json').read_text()) for directory in (old, new)]
        assert records[0] and records[0] == records[1], (name, 'guest hashes/cycles/metadata changed')
        identities = [json.loads((directory/'identity.json').read_text()) for directory in (old, new)]
        for key in ('state_sha256', 'sequence', 'host_width', 'objects', 'general_fields', 'full_combat'):
            assert identities[0][key] == identities[1][key], (name, key)
        before, after = field_frames(old), field_frames(new)
        assert len(before) == len(after) and len(records[0])-1 <= len(after) <= len(records[0])
        assert all(a['completed'] == b['completed'] for a, b in zip(before, after))
        changed = sum(a['wide'] != b['wide'] for a, b in zip(before, after))
        assert changed == recovered, (name, changed, recovered)
        if kind == 'bow':
            assert all(row['wide'] == '1' for row in after), (name, 'bow still narrows')
            assert all(row['reason'] == 'lake-player-control' for row in before if row['wide'] == '0')
        else:
            assert before == after, (name, 'unrelated field policy changed')
        if kind in ('field', 'tool'):
            assert any(row['wide'] == '1' for row in after), (name, 'never widened')
        if kind == 'script':
            assert all(row['wide'] == '0' for row in after), (name, 'script framing lost')
        # Combat ownership/routing is unaffected even though field declines it.
        if kind == 'battle':
            lines = [[line for line in (directory/'stderr.log').read_text().splitlines()
                      if '[sc3:state-frame]' in line or '[sc3:composition]' in line]
                     for directory in (old, new)]
            assert lines[0] == lines[1] and any('complete_owner=1' in line for line in lines[1])
        report.append(dict(case=name, frames=len(records[0]), recovered_wide_frames=changed,
                           wide_frames=sum(row['wide'] == '1' for row in after),
                           guest_state_and_cycles_unchanged=True, identities=identities))
        (out/'report.json').write_text(json.dumps(report, indent=2))
        print(f'PASS {name}: {len(records[0])} paired frames, {changed} handoff frames recovered', flush=True)
    print(f'PASS {sum(row["frames"] for row in report)} paired frames; objects={args.objects}', flush=True)


if __name__ == '__main__':
    main()
