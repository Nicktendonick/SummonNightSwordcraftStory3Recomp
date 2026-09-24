"""Validate signed spell-window extension from the owner's edge capture.

State/register/source-permission checks only; no screenshot or framebuffer reads.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from probe_battle_state import ROOT
from validate_spell_windows import traces


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--verify-existing', action='store_true')
    args = p.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(ROOT/'validation')
    state = ROOT/'validation/playtest-20260922-160731-035/frame-0000015765-1790107822613/state.gbas'
    sequence = '1023:120'
    if not args.verify_existing:
        output.mkdir(exist_ok=False)
    for version, exe in [('before', args.before.resolve()),
                         ('after', ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
        dest = output/version
        if args.verify_existing:
            identity = json.loads((dest/'identity.json').read_text())
            assert identity['executable_sha256'] == hashlib.sha256(exe.read_bytes()).hexdigest()
            assert identity['state_sha256'] == hashlib.sha256(state.read_bytes()).hexdigest()
            assert identity['sequence'] == sequence and identity['window_audit']
        else:
            subprocess.run([sys.executable, '-B', str(ROOT/'tools/probe_battle_state.py'),
                '--state', str(state), '--exe', str(exe), '--output', str(dest),
                '--sequence', sequence, '--compact', '--field-audit', '--general-fields',
                '--window-audit'], check=True)
    old, new = [json.loads((output/v/'frames.json').read_text()) for v in ('before', 'after')]
    assert len(new) == 120 and old == new, 'recorded guest state changed'
    a, sa = traces(output/'before')
    b, sb = traces(output/'after')
    assert a.keys() == b.keys() == {(f, y) for f in range(1, 120) for y in range(160)}
    assert sa == sb and list(sb) == list(range(1, 120))
    assert all(s['active'] == '1' and s['wide'] == '1' for s in sb.values())
    expanded_rows = expanded_samples = 0
    expanded_frames = set()
    for key, row in b.items():
        prev = a[key]
        assert all(row[k] == v for k, v in prev.items()
                   if k not in {'left_samples', 'right_samples'}), (key, 'raster metadata changed')
        assert prev['left_samples'] == prev['right_samples'] == 0
        valid = row['spell_window'] != 0
        if valid:
            assert (row['spell_left'], row['spell_right'], row['spell_top'], row['spell_bottom']) == (-444, 69, 40, 296)
            assert row['bg2'] == 0x4305 and row['hofs'] == 443 and row['vofs'] == 216
            assert row['win0h'] == 0x0045 and row['win0v'] == 0x28ff
        permitted = valid and row['bg0'] == 0x470b and 40 <= row['y'] < 125
        assert row['left_samples'] == (72 if permitted else 0), (key, 'left source eligibility')
        assert row['right_samples'] == 0, (key, 'opposite-edge duplicate')
        if permitted:
            expanded_rows += 1
            expanded_samples += row['left_samples']
            expanded_frames.add(key[0])
    assert expanded_frames == set(range(1, 41)), 'missing cast or teardown transition'
    assert all(not row['spell_window'] for (f, _), row in b.items() if f >= 41)
    report = dict(frames=len(new), complete_frames=len(sb), recorded_guest_state_unchanged=True,
        captured_registers_unchanged=True, expanded_frames=len(expanded_frames),
        expanded_gameplay_rows=expanded_rows, permitted_left_source_samples=expanded_samples,
        opposite_edge_samples=0, all_battle_owned_frames_wide=True,
        cleanup_retires_extension=True,
        note='Source eligibility, not opaque/visible pixel counts or a full CPU-state equivalence test.')
    (output/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
