"""Paired TCP state/register checks for moving spell display epochs; no pixels."""
import argparse
import json
from pathlib import Path

from validate_spell_windows import traces


def compare(before, after, flare=True):
    a, b = [json.loads((p/'frames.json').read_text()) for p in (before, after)]
    assert a == b and a, 'guest hashes/cycles or endpoint metadata changed'
    identities = [json.loads((p/'identity.json').read_text()) for p in (before, after)]
    for k in ('state_sha256', 'sequence', 'host_width', 'objects', 'general_fields'):
        assert identities[0][k] == identities[1][k], k
    assert identities[1]['host_width'] == 384
    old, os = traces(before)
    new, ns = traces(after)
    assert old.keys() == new.keys() and os == ns
    policy = {'spell_window', 'spell_left', 'spell_right', 'spell_top', 'spell_bottom',
              'spell_kind', 'margin_layers', 'effect_policy', 'left_samples', 'right_samples'}
    active, dropped_before, dropped_after = set(), set(), set()
    changed = 0
    for key, row in new.items():
        prev = old[key]
        assert {k: v for k, v in row.items() if k not in policy} == {
            k: v for k, v in prev.items() if k not in policy}, (key, 'raster/source changed')
        changed += row != prev
        if 40 <= row['y'] < 125 and row['display'] & 0x400 and row['bg2'] == 0x4305:
            active.add(key[0])
            if not prev['spell_window']:
                dropped_before.add(key[0])
            if not row['spell_window']:
                dropped_after.add(key[0])
        if row['spell_window'] and row['display'] & 0x400:
            for name, xs in [('left_samples', range(-72, 0)), ('right_samples', range(240, 312))]:
                count = sum(row['spell_left'] <= x < row['spell_right'] for x in xs)
                if not (row['bg0'] == 0x470b and row['spell_top'] <= row['y'] < min(125, row['spell_bottom'])):
                    count = 0
                assert row[name] == count, (key, name, row[name], count)
    if flare:
        assert active and not dropped_after, (after.name, dropped_after)
    else:
        lost = [k for k, row in new.items() if old[k]['spell_window'] and not row['spell_window']]
        assert not lost, (after.name, 'previously accepted windows lost', lost[:8])
    assert all(s['wide'] == '1' for s in ns.values() if s['active'] == '1')
    return dict(case=after.name, frames=len(a), active_frames=len(active),
                dropped_before=len(dropped_before), dropped_after=len(dropped_after),
                changed_rows=changed, guest_and_registers_unchanged=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--after', type=Path, required=True)
    p.add_argument('--regressions', action='store_true')
    args = p.parse_args()
    names = [p.name for p in sorted(args.before.iterdir()) if p.is_dir()] if args.regressions else ('neutral', 'left', 'right', 'turns')
    report = [compare(args.before/n, args.after/n, not args.regressions) for n in names]
    (args.after/'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
