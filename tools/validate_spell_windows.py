"""State-only paired spell-window and teardown regression at 12:5.

No screenshot/framebuffer commands. The raster trace records source permissions
and callback reachability, not whether a tile produced an opaque pixel.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from probe_battle_state import ROOT

HEX = {'display', 'bg0', 'bg1', 'bg2', 'win0h', 'win1h', 'win0v', 'win1v',
       'winin', 'winout', 'blend', 'alpha'}


def traces(directory):
    raster, states = {}, {}
    with (directory/'stderr.log').open() as source:
        for line in source:
            if '[sc3:battle-window]' in line:
                row = {k: int(v, 16 if k in HEX else 10)
                       for k, v in re.findall(r'(\w+)=([^ ]+)', line.strip())}
                key = (row['completed'], row['y'])
                assert key not in raster
                raster[key] = row
            elif '[sc3:state-frame]' in line:
                row = dict(re.findall(r'(\w+)=([^ ]+)', line.strip()))
                completed = int(row['completed'])
                assert completed not in states, ('duplicate state frame', completed)
                states[completed] = row
    return raster, states


def effect_interval(hofs, span):
    if not 0 < span <= 256:
        return None
    a = (-hofs) & 255
    b = a-256
    overlap = lambda origin: max(0, min(240, origin+span)-max(0, origin))
    if overlap(a) == overlap(b):
        return None
    origin = a if overlap(a) > overlap(b) else b
    return origin, origin+span


def run(state, exe, output, sequence):
    subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
        '--state', str(state), '--exe', str(exe), '--output', str(output),
        '--sequence', sequence, '--compact', '--field-audit', '--general-fields',
        '--window-audit'], check=True, creationflags=subprocess.CREATE_NO_WINDOW)


def check_case(old, new):
    a, b = [json.loads((d/'frames.json').read_text()) for d in (old, new)]
    assert len(a) == len(b) and a
    assert a == b, 'guest state/metadata changed'
    before, old_states = traces(old)
    after, new_states = traces(new)
    assert before and before.keys() == after.keys()
    assert old_states.keys() == new_states.keys()
    # The initial read precedes a completed raster; every subsequent frame
    # must have a consecutive state trace in both builds, not just equal gaps.
    assert list(new_states) == list(range(1, len(a)))
    expected_rows = {(f, y) for f, s in new_states.items()
                     if s['active'] == '1' for y in range(160)}
    assert after.keys() == expected_rows
    for completed in new_states:
        rows = [y for f, y in after if f == completed]
        if new_states[completed]['active'] == '1':
            assert rows == list(range(160)), (completed, 'incomplete raster trace')
            assert new_states[completed]['wide'] == '1', (completed, new_states[completed])
        else:
            # A restored state can precede the first ownership-hook callback.
            assert completed == 1 and old_states[completed] == new_states[completed]
    suppressed = 0
    leaked = 0
    cleanup_rows = 0
    canvas_rows = 0
    active_spell_rows = 0
    signed_window_rows = 0
    signed_window_samples = 0
    for key, row in after.items():
        prev = before[key]
        for field, value in prev.items():
            if field not in {'authored', 'layout'}:
                assert row[field] == value, (key, field, 'captured register/source changed')
        assert prev['authored'] == 15 and row['authored'] == 3
        signed_window = row.get('spell_window', 0) != 0
        if signed_window:
            signed_window_rows += 1
            signed_window_samples += row['left_samples'] + row['right_samples']
        if (row['display'] & 7) == 0 and row['bg2'] == 0x4305:
            assert prev['layout'] == 0 and row['layout'] == 1
            if not signed_window:
                assert row['left_samples'] == row['right_samples'] == 0, (key, 'untraced 512-wide canvas escaped native scope')
            canvas_rows += 1
            if not row['display'] & 0x400:
                cleanup_rows += 1
        # The original hardware rectangle leaves margins in WINOUT. Only a
        # separately authenticated signed rectangle may continue WIN0 there.
        window_forbids = bool(row['display'] & 0xe000) and not (row['winout'] & 4)
        if signed_window:
            # Only the authenticated signed rectangle may select WIN0 in the
            # margin; this is an eligibility assertion, not a pixel comparison.
            for field, xs in [('left_samples', range(-72, 0)), ('right_samples', range(240, 312))]:
                permitted = sum(row['spell_left'] <= x < row['spell_right'] for x in xs)
                if not (row['bg0'] == 0x470b and row['spell_top'] <= row['y'] < min(125, row['spell_bottom'])):
                    permitted = 0
                assert row[field] == permitted, (key, field, 'signed-window eligibility changed')
        elif window_forbids:
            assert row['left_samples'] == row['right_samples'] == 0, (key, 'masked effect sample authorized')
            suppressed += 1
        if row['display'] & 0x400 and (row['display'] & 7) == 0 and row['bg0'] == 0x470b:
            active_spell_rows += 1
            interval = effect_interval(row['hofs'], row['span'])
            # The old path would allow these source coordinates even though
            # WINOUT forbids BG2. This is a permission leak, not a pixel count.
            if window_forbids and interval and old_states[key[0]]['wide'] == '1':
                leaked += sum(interval[0] <= x < interval[1] for x in range(-72, 0))
    return dict(frames=len(a), complete_frames=len(new_states), guest_state_unchanged=True,
        captured_registers_unchanged=True,
        old_narrow=sum(s['wide'] == '0' for s in old_states.values()),
        new_narrow=sum(s['wide'] == '0' for s in new_states.values()),
        masked_rows_with_zero_effect_samples=suppressed,
        formerly_authorized_left_samples=leaked,
        disabled_canvas_rows_now_accepted=cleanup_rows, setup_cleanup_canvas_rows=canvas_rows,
        authenticated_signed_window_rows=signed_window_rows,
        authenticated_signed_window_samples=signed_window_samples,
        active_spell_rows=active_spell_rows,
        old_reasons=dict(Counter(s['reason'] for s in old_states.values())))


def check_input_effect_transition(directory):
    rows, states = traces(directory)
    assert states[1]['mode'] == '2'
    effects = [f for (f, y), r in rows.items() if y == 60
               and states[f]['mode'] == '7' and r['display'] & 0x400
               and (r['display'] & 7) == 0 and r['bg2'] == 0x0305
               and r['span'] == 240 and r['win0v'] == 0x28a0
               and r['winin'] == 0x3f37 and r['winout'] == 0x5513
               and r['blend'] == 0x1344 and r['alpha'] == 0x1010]
    assert effects and min(effects) > 1, 'no new reviewed effect transition'
    cleanup = [f for (f, y), r in rows.items() if y == 60
               and f > max(effects) and r['bg2'] == 0x4305
               and not (r['display'] & 0x400)]
    assert cleanup, 'no disabled setup/cleanup canvas after effect'
    assert any(f >= min(cleanup) and s['mode'] == '2'
               for f, s in states.items()), 'did not return to battle mode 2'
    # A traced effect transition, not an independently decoded spell-name ID.
    return dict(first_effect_frame=min(effects), last_effect_frame=max(effects),
                first_cleanup_frame=min(cleanup), returned_to_mode2=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True, help='Trace-only baseline executable')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--verify-existing', action='store_true')
    a = p.parse_args()
    output = a.output.resolve()
    assert output.is_relative_to(ROOT/'validation')
    if a.verify_existing:
        assert output.is_dir()
    else:
        output.mkdir(exist_ok=False)
    base = ROOT/'validation/playtest-20260922-145648-344'
    cases = [('cast', 'frame-0000045213-1790104030258', '1023:96'),
             ('cleanup', 'frame-0000044775-1790104006966', '1023:96'),
             ('late-cleanup', 'frame-0000045254-1790104030976', '1023:96'),
             ('mid-cast', 'frame-0000045231-1790104014754', '1023:96'),
             ('precast-b', 'frame-0000044708-1790104005786', '1021:120,1023:30')]
    summary = []
    for name, folder, sequence in cases:
        state = base/folder/'state.gbas'
        for version, exe in [('before', a.before.resolve()), ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            destination = output/name/version
            if a.verify_existing:
                identity = json.loads((destination/'identity.json').read_text())
                assert identity['executable_sha256'] == hashlib.sha256(exe.read_bytes()).hexdigest()
                assert identity['state_sha256'] == hashlib.sha256(state.read_bytes()).hexdigest()
                assert identity['sequence'] == sequence and identity['window_audit']
            else:
                run(state, exe, destination, sequence)
        result = dict(case=name, **check_case(output/name/'before', output/name/'after'))
        if name == 'cast':
            assert result['formerly_authorized_left_samples'] > 0 and result['disabled_canvas_rows_now_accepted'] > 0
        if name == 'precast-b':
            old_transition = check_input_effect_transition(output/name/'before')
            result['input_effect_transition'] = check_input_effect_transition(output/name/'after')
            assert old_transition == result['input_effect_transition']
        summary.append(result)
        (output/'report.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
        print(json.dumps(result), flush=True)
    print('PASS spell window regression', sum(s['frames'] for s in summary), 'paired frames', flush=True)


if __name__ == '__main__':
    main()
