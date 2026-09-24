"""State-only regressions for field selection and inactive window permissions.

Reuses explicitly named paired probe outputs; never reads screenshots/pixels.
"""
import argparse
import json
from pathlib import Path

from probe_battle_state import ROOT
from validate_field_tools import field_frames
from validate_spell_windows import traces


def compare(old, new, field=False):
    a, b = [json.loads((p/'frames.json').read_text()) for p in (old, new)]
    assert a and a == b, (new.name, 'guest hashes, cycles or metadata changed')
    identities = [json.loads((p/'identity.json').read_text()) for p in (old, new)]
    for key in ('state_sha256', 'sequence', 'host_width', 'objects'):
        assert identities[0][key] == identities[1][key]
    assert identities[1]['host_width'] == 384
    before, old_states = traces(old)
    after, states = traces(new)
    assert before.keys() == after.keys() and old_states.keys() == states.keys()
    for completed, state in states.items():
        previous = old_states[completed]
        ignored = {'wide'} if field else set()
        assert {k: v for k, v in previous.items() if k not in ignored} == {
            k: v for k, v in state.items() if k not in ignored}
    changed = 0
    policy_fields = {'spell_window', 'spell_left', 'spell_right', 'spell_top',
                     'spell_bottom', 'spell_kind', 'effect_policy', 'margin_layers',
                     'left_samples', 'right_samples'}
    for key, row in after.items():
        prev = before[key]
        assert {k: v for k, v in prev.items() if k not in policy_fields} == {
            k: v for k, v in row.items() if k not in policy_fields}
        if row != prev:
            changed += 1
            assert not prev['spell_window'] and row['spell_window']
            assert (row['winin'] & 63) == 0x37 and (row['winout'] & 63) == 0x13
            assert (row['display'] & 0xe000) == 0x2000
        if row['spell_window'] and row['display'] & 0x400:
            for name, xs in [('left_samples', range(-72, 0)),
                             ('right_samples', range(240, 312))]:
                count = sum(row['spell_left'] <= x < row['spell_right'] for x in xs)
                if not (row['bg0'] == 0x470b and row['spell_top'] <= row['y'] < min(125, row['spell_bottom'])):
                    count = 0
                assert row[name] == count, (key, name, row[name], count)
    result = dict(case=new.name, frames=len(a), guest_state_and_cycles_unchanged=True,
                  registers_unchanged=True, changed_window_rows=changed,
                  before=identities[0], after=identities[1])
    if field:
        f, g = field_frames(old), field_frames(new)
        assert len(f) == len(g) and len(a)-1 <= len(g) <= len(a)
        assert all(x['completed'] == y['completed'] for x, y in zip(f, g))
        assert all(x['wide'] == '1' for x in g), 'field selection still narrowed'
        count = sum(x['wide'] == '0' for x in f)
        assert count > 0, 'no formerly narrowed selection frames exercised'
        callbacks = {s['callback'] for frame in b for s in frame['field_actions']['actions']}
        assert '0x809d859' in callbacks
        result.update(formerly_narrow_frames=count, complete_wide_frames=len(g))
    else:
        assert after
        assert len(states) >= len(a)-1
        for completed, state in states.items():
            if state['active'] == '1':
                assert state['wide'] == '1'
                assert [(f, y) for f, y in after if f == completed] == [(completed, y) for y in range(160)]
            else:
                assert completed == 1, 'unexpected loss of battle ownership'
        result.update(before_margin_candidates=sum(x['left_samples']+x['right_samples'] for x in before.values()),
                      after_margin_candidates=sum(x['left_samples']+x['right_samples'] for x in after.values()))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence.resolve()
    assert root.is_relative_to(ROOT/'validation')
    report = []
    for old in sorted((root/'baseline').iterdir()):
        if not old.is_dir():
            continue
        field = old.name.startswith(('frame-0000036309-', 'frame-0000045520-'))
        result = compare(old, root/'after'/old.name, field)
        if old.name.startswith(('frame-0000008915-', 'frame-0000009524-')):
            assert result['changed_window_rows'] > 0 and result['after_margin_candidates'] > 0
        elif not field:
            assert result['changed_window_rows'] == 0
        report.append(result)
    assert len(report) == 9
    if (root/'selection-routes').exists():
        for old in sorted((root/'selection-routes').glob('*-before')):
            report.append(compare(old, old.with_name(old.name[:-7]+'-after'), True))
    (root/'report.json').write_text(json.dumps(report, indent=2))
    print('PASS', len(report), 'cases;', sum(r['frames'] for r in report),
          'paired frames; guest hashes/cycles/registers unchanged; no pixels')


if __name__ == '__main__':
    main()
