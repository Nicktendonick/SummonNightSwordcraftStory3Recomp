"""Paired, state-only 12:5 validation of the actor-owned casting window.

Uses isolated runs through the existing battle-state probe. It never requests
screenshots or framebuffer data. Sample counts measure permitted BG2 source
candidates, not opaque pixels or an independent CPU equivalence proof.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from probe_battle_state import ROOT
from validate_spell_windows import check_input_effect_transition, traces


LATEST = ROOT/'validation/playtest-20260922-164002-290'
OLDER = ROOT/'validation/playtest-20260922-145648-344'
EDGE = ROOT/'validation/playtest-20260922-160731-035'
CASES = [
    ('latest-casting', LATEST/'frame-0000061488-1790109682039/state.gbas', '1023:180'),
    ('latest-cleanup', LATEST/'frame-0000061517-1790109680143/state.gbas', '1023:120'),
    ('casting-pause', LATEST/'frame-0000061488-1790109682039/state.gbas',
     '1023:2,1015:1,1023:30,1015:1,1023:50'),
    ('older-cast', OLDER/'frame-0000045213-1790104030258/state.gbas', '1023:96'),
    ('precast-b', OLDER/'frame-0000044708-1790104005786/state.gbas', '1021:120,1023:30'),
    ('type2-left-edge', EDGE/'frame-0000015765-1790107822613/state.gbas', '1023:120'),
    # Together with latest-casting, these give three independent restorations
    # of the first 60 frames. A nondeterministic result fails rather than
    # silently authorizing guest differences for a presentation-only change.
    ('latest-repeat-2', LATEST/'frame-0000061488-1790109682039/state.gbas', '1023:60'),
    ('latest-repeat-3', LATEST/'frame-0000061488-1790109682039/state.gbas', '1023:60'),
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def frame_count(sequence):
    return sum(int(part.split(':')[1]) for part in sequence.split(','))


def run(state, exe, output, sequence, draw_audit=False):
    command = [sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
        '--state', str(state), '--exe', str(exe), '--output', str(output),
        '--sequence', sequence, '--compact', '--general-fields', '--window-audit']
    if draw_audit:
        command.append('--draw-audit')
    subprocess.run(command, check=True,
                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))


def check_identity(directory, state, exe, sequence):
    identity = load_json(directory/'identity.json')
    assert identity['executable_sha256'] == sha256(exe), (directory, 'build identity')
    assert identity['state_sha256'] == sha256(state), (directory, 'source state identity')
    assert Path(identity['state']).resolve() == state.resolve()
    assert Path(identity['executable']).resolve() == exe.resolve()
    assert identity['sequence'] == sequence
    assert identity['host_width'] == 384 and identity['frames'] == frame_count(sequence)
    assert identity['objects'] == 'enabled' and identity['general_fields']
    assert identity['window_audit'] and not identity['field_audit'] and not identity['tool_audit']
    return identity


def policy_field(name):
    return name.startswith('spell_') or name in {'left_samples', 'right_samples', 'margin_layers', 'effect_policy'}


def native_window(a, b):
    # The guest setter clamps each signed endpoint to 0..255 before packing.
    return (max(0, min(255, a)) << 8) | max(0, min(255, b))


def expected_candidates(row, xs):
    if not row['spell_window'] or not (row['display'] & 0x400):
        return 0
    if row['bg0'] != 0x470b or not row['spell_top'] <= row['y'] < min(125, row['spell_bottom']):
        return 0
    return sum(row['spell_left'] <= x < row['spell_right'] for x in xs)


def check_pair(directory, count):
    old, new = [load_json(directory/version/'frames.json') for version in ('before', 'after')]
    assert len(old) == len(new) == count
    assert old == new, (directory, 'guest RAM/resource hashes, cycles, input, or ownership changed')
    before, old_states = traces(directory/'before')
    after, new_states = traces(directory/'after')
    assert old_states == new_states, (directory, 'battle ownership or wide coverage changed')
    assert list(new_states) == list(range(1, count)), (directory, 'missing completed frame')
    expected_rows = {(f, y) for f, state in new_states.items()
                     if state['active'] == '1' for y in range(160)}
    assert before.keys() == after.keys() == expected_rows, (directory, 'incomplete raster trace')
    for frame, state in new_states.items():
        if state['active'] == '1':
            assert state['wide'] == '1', (directory, frame, 'scenery stopped being wide')
        else:
            assert frame == 1, (directory, frame, 'unexpected loss of ownership')

    kinds = Counter()
    new_right = new_left = 0
    disabled_window_rows = 0
    for key, row in after.items():
        previous = before[key]
        assert {k: v for k, v in previous.items() if not policy_field(k)} == {
            k: v for k, v in row.items() if not policy_field(k)}, (directory, key, 'register/source schedule changed')
        assert row['authored'] == previous['authored'] == 3
        assert row['layout'] == previous['layout'] == 1
        assert 'spell_kind' in row, (directory, 'new build is missing kind diagnostics')
        valid = row['spell_window'] != 0
        kind = row['spell_kind'] if valid else 0
        if valid:
            assert kind in {2, 3, 4}, (directory, key, 'unknown window owner')
            kinds[kind] += 1
            assert native_window(row['spell_left'], row['spell_right']) == row['win0h']
            assert native_window(row['spell_top'], row['spell_bottom']) == row['win0v']
            assert row['display'] & 0x2000 and not (row['display'] & 0xc087)
            assert row['bg2'] in {0x0305, 0x4305}
            assert row['winin'] == 0x3f37 and row['winout'] == 0x5513
            assert row['blend'] == 0x1344
            assert row['left_samples'] == expected_candidates(row, range(-72, 0)), (directory, key, 'left eligibility')
            assert row['right_samples'] == expected_candidates(row, range(240, 312)), (directory, key, 'right eligibility')
        if kind in {3, 4}:
            assert row['bg2'] == 0x0305
            assert row['spell_right'] - row['spell_left'] == 120
            assert row['spell_bottom'] - row['spell_top'] == 120
            assert previous['spell_window'] == 0
            assert previous['left_samples'] == previous['right_samples'] == 0
            new_left += row['left_samples']
            new_right += row['right_samples']
            if not (row['display'] & 0x400):
                disabled_window_rows += 1
                assert row['left_samples'] == row['right_samples'] == 0
        else:
            # Existing type-2 signed windows and unsupported fallback behavior
            # must remain identical. The extra kind field is new diagnostics.
            for field, value in previous.items():
                if field != 'spell_kind':
                    assert row[field] == value, (directory, key, field, 'non-casting policy changed')
    return dict(frames=count, completed_frames=len(new_states),
        guest_hashes_cycles_and_ownership_unchanged=True, captured_registers_unchanged=True,
        all_battle_owned_frames_wide=True, window_rows_by_kind=dict(kinds),
        new_casting_left_candidates=new_left, new_casting_right_candidates=new_right,
        bg2_disabled_casting_window_rows=disabled_window_rows)


def check_latest(directory, count):
    rows, _ = traces(directory/'after')
    casting = set()
    emitting = set()
    type2 = set()
    for (frame, y), row in rows.items():
        if row['spell_window'] and row['spell_kind'] in {3, 4}:
            casting.add(frame)
            assert row['spell_kind'] == 3
            assert (row['spell_left'], row['spell_right'], row['spell_top'], row['spell_bottom']) == (164, 284, 40, 160)
            assert row['left_samples'] == 0, (frame, y, 'opposite-edge casting duplicate')
            permitted = frame <= 19 and 40 <= y < 125
            assert row['right_samples'] == (44 if permitted else 0), (frame, y, 'reported right-edge eligibility')
            assert bool(row['display'] & 0x400) == (frame <= 19)
            if row['right_samples']:
                emitting.add(frame)
        elif row['spell_window'] and row['spell_kind'] == 2:
            type2.add(frame)
    assert casting == set(range(1, 40)), 'casting window must follow the live casting owner'
    assert emitting == set(range(1, 20)), 'missing initial casting frame or late ghost candidates'
    assert all(not r['spell_window'] for (f, _), r in rows.items() if 40 <= f < min(count, 140))
    if count == 180:
        assert type2 == set(range(140, 180)), 'existing follow-up spell window changed'
    retirement = check_casting_retirement(directory)
    return dict(casting_window_frames=39, bg2_emitting_frames=19,
                new_right_candidates=19*85*44, opposite_left_candidates=0,
                disabled_bg2_has_no_candidates=True, window_retires_at_frame=40,
                retirement_evidence=retirement)


def check_casting_retirement(directory):
    """Prove retirement from guest owner, live IO and OAM attributes, not art.

    Completed frame 40 has handed ownership from kind 3 to kind 2/state 0.
    BG2 is already disabled, there are no enabled semitransparent objects, and
    old WIN0 merely awaits its next-frame disable. It must not inherit the old
    caster rectangle after that owner ceases to exist.
    """
    for version in ('before', 'after'):
        capture = directory/version
        iwram = (capture/'0040-iwram.bin').read_bytes()
        oam = (capture/'0040-oam.bin').read_bytes()
        assert len(iwram) == 0x8000 and len(oam) == 0x400
        u16 = lambda data, offset: int.from_bytes(data[offset:offset+2], 'little')
        assert iwram[0x44e] == 2 and iwram[0x44f] == 0
        assert u16(iwram, 0x2990) == 0x1340
        semitransparent = []
        for number in range(128):
            attr0 = u16(oam, number*8)
            affine = bool(attr0 & 0x100)
            enabled = affine or not bool(attr0 & 0x200)
            valid_shape = (attr0 >> 14) != 3
            if enabled and valid_shape and (attr0 & 0xc00) == 0x400:
                semitransparent.append(number)
        assert not semitransparent, (version, 'casting objects still own the final window', semitransparent)
        rows, _ = traces(capture)
        for y in range(160):
            pending, retired = rows[(40, y)], rows[(41, y)]
            assert pending['display'] & 0x2400 == 0x2000, (version, y, 'expected BG2-off/WIN0-on handoff')
            assert retired['display'] & 0x2400 == 0, (version, y, 'WIN0 did not retire')
            assert pending['left_samples'] == pending['right_samples'] == 0
            assert not pending['spell_window']
    return dict(owner_kind=2, owner_state=0, shadow_display='1340',
                enabled_semitransparent_objects=0, live_bg2_disabled=True,
                old_win0_retires_next_frame=True)


def check_casting_start(directory):
    # 080270AC's mode7 branch (080271B0) bypasses the ordinary Start handler
    # at 08027224. That handler additionally requires effect kind1. Casting
    # kinds3/4 must not become pausable merely because presentation is wider.
    for version in ('before', 'after'):
        capture = directory/version
        frames = load_json(capture/'frames.json')
        _, states = traces(capture)
        assert [f['index'] for f in frames if f['key'] == 0x03f7] == [2, 33]
        assert all(frames[n]['battle']['mode'] == 7 for n in (2, 33))
        assert all(f['battle']['pause'] == 0 for f in frames)
        assert all(s['pause'] == '0' and s['top'] == '19' for s in states.values())
    rows, states = traces(directory/'after')
    assert any(row['spell_window'] and row['spell_kind'] in {3, 4} for row in rows.values())
    assert all(row['left_samples'] == row['right_samples'] == 0
               for (frame, y), row in rows.items() if y < int(states[frame]['top']))
    return dict(start_input_indices=[2, 33], casting_start_intentionally_ignored=True,
                guest_pause_behavior_unchanged=True, hud_rows_have_no_bg2_candidates=True,
                note='Normal pause is covered separately; this is not a pause/unpause test.')


def check_left_edge(directory):
    rows, _ = traces(directory/'after')
    active = set()
    for (frame, y), row in rows.items():
        if row['spell_window']:
            active.add(frame)
            assert row['spell_kind'] == 2
            assert (row['spell_left'], row['spell_right'], row['spell_top'], row['spell_bottom']) == (-444, 69, 40, 296)
            assert row['left_samples'] == (72 if 40 <= y < 125 else 0)
            assert row['right_samples'] == 0
    assert active == set(range(1, 41))
    return dict(type2_window_frames=40, left_candidates=40*85*72, unchanged=True)


def check_repeats(output):
    for version in ('before', 'after'):
        first = output/'latest-casting'/version
        reference = load_json(first/'frames.json')[:60]
        raster, states = traces(first)
        raster = {key: row for key, row in raster.items() if key[0] < 60}
        states = {key: row for key, row in states.items() if key < 60}
        for name in ('latest-repeat-2', 'latest-repeat-3'):
            directory = output/name/version
            assert load_json(directory/'frames.json') == reference, (name, version, 'restoration nondeterminism')
            assert traces(directory) == (raster, states), (name, version, 'trace nondeterminism')
    return dict(independent_restorations=3, compared_frames_per_run=60,
                guest_and_policy_deterministic=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True, help='Preserved pre-casting-extension executable')
    p.add_argument('--output', type=Path, required=True)
    reuse = p.add_mutually_exclusive_group()
    reuse.add_argument('--verify-existing', action='store_true', help='Verify recorded data without launching a game')
    reuse.add_argument('--resume', action='store_true',
                       help='Reuse identity-validated completed runs; launch only missing runs; reject partial directories')
    args = p.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(ROOT/'validation') and output != ROOT/'validation'
    if args.verify_existing or args.resume:
        assert output.is_dir()
    else:
        output.mkdir(parents=True, exist_ok=False)
    before = args.before.resolve()
    after = ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
    report = dict(host_width=384, before_executable_sha256=sha256(before),
        after_executable_sha256=sha256(after),
        validator_source_sha256={name: sha256(ROOT/'tools'/name) for name in (
            'validate_casting_extension.py', 'probe_battle_state.py', 'validate_spell_windows.py')},
        cases=[], note='State/register/source eligibility only; no pixels or independent CPU oracle.')
    for name, state, sequence in CASES:
        directory = output/name
        for version, exe in (('before', before), ('after', after)):
            destination = directory/version
            reuse_completed = args.resume and destination.exists()
            if reuse_completed:
                # Identity is emitted only after the probe has exited cleanly.
                # Never delete/rewrite an interrupted capture or silently treat
                # it as complete. Verify all existing identity fields below.
                assert destination.is_dir() and (destination/'identity.json').is_file(), (
                    destination, 'partial capture cannot be resumed; preserve it and choose a new output directory')
            if not args.verify_existing and not reuse_completed:
                run(state, exe, destination, sequence,
                    draw_audit=name in {'latest-casting', 'latest-repeat-2', 'latest-repeat-3'})
            check_identity(destination, state, exe, sequence)
        result = dict(case=name, source_state_sha256=sha256(state),
                      **check_pair(directory, frame_count(sequence)))
        if name.startswith('latest-') and name != 'latest-cleanup':
            result['reported_edge'] = check_latest(directory, frame_count(sequence))
            assert result['new_casting_right_candidates'] == 19*85*44
            assert result['new_casting_left_candidates'] == 0
        elif name == 'latest-cleanup':
            assert result['bg2_disabled_casting_window_rows'] > 0
            assert result['new_casting_left_candidates'] == result['new_casting_right_candidates'] == 0
        elif name == 'casting-pause':
            result['casting_start'] = check_casting_start(directory)
        elif name == 'older-cast':
            assert sum(result['window_rows_by_kind'].get(kind, 0) for kind in (3, 4)) > 0
            assert result['new_casting_left_candidates'] == result['new_casting_right_candidates'] == 0
        elif name == 'precast-b':
            old_transition = check_input_effect_transition(directory/'before')
            result['input_effect_transition'] = check_input_effect_transition(directory/'after')
            assert old_transition == result['input_effect_transition']
            assert sum(result['window_rows_by_kind'].get(kind, 0) for kind in (3, 4)) > 0
        elif name == 'type2-left-edge':
            result['prior_regression'] = check_left_edge(directory)
            assert result['new_casting_left_candidates'] == result['new_casting_right_candidates'] == 0
        report['cases'].append(result)
        (output/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(json.dumps(result), flush=True)
    report['repeatability'] = check_repeats(output)
    report['paired_frames'] = sum(case['frames'] for case in report['cases'])
    report['pass'] = True
    (output/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS casting extension:', report['paired_frames'], 'paired state-only frames', flush=True)


if __name__ == '__main__':
    main()
