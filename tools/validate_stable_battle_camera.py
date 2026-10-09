"""Private same-ROM A/B camera regression. State/register assertions, no pixels.

Uses the installed Camera Edge Test as the old Follow baseline. Runs only in
isolated diagnostic folders and never edits supplied captures, saves or ROMs.
"""
import argparse
import json
from pathlib import Path
import time

import validate_follow_edge_camera as matrix
from package_alpha import file_hash
from promote_portable_beta import no_game_running
from update_guard_performance import snapshot

ROOT, OWNER = matrix.ROOT, matrix.OWNER
TEST = OWNER / 'release/Portable Camera Edge Test'
OLD = TEST / 'Runtime/Swordcraft3CustomRendererBeta.exe'
NEW = ROOT / 'build-native/Swordcraft3Translation106.exe'
ROM = matrix.NEW_ROM
FIXTURES = ROOT / 'validation/translation106-input-audit-1790824852616961300/diagnostic-fixtures'
ROUTE = (
    ('settle', 0x3ff, 4), ('left', 0x3df, 180),
    ('guard', 0x3fb, 12), ('guard-release', 0x3ff, 8),
    ('right', 0x3ef, 240), ('jump-right', 0x3af, 30), ('right', 0x3ef, 100),
    ('right-release', 0x3ff, 8),
    ('jump-right-still', 0x3bf, 30), ('land-right', 0x3ff, 60),
    ('pause', 0x3f7, 1), ('paused', 0x3ff, 8),
    ('resume', 0x3f7, 1), ('release', 0x3ff, 12),
)


def verify_stability(folder, rows, old_folder, edge_limits=(('right', 128),)):
    camera = matrix.traces((folder / 'stderr.log').read_text(errors='replace'), 'battle-camera')
    old = matrix.traces((old_folder / 'stderr.log').read_text(errors='replace'), 'battle-camera')
    active = [t for t in camera if t['active']]
    assert active, 'Stable Follow never engaged'
    anchors = {}
    for t in active:
        flat = t['h_flat']
        anchors.setdefault(flat, t['anchor'])
        assert anchors[flat] == t['anchor'], 'Jump extrema changed the horizontal anchor'
        assert t['envelope_min'] <= t['h_min'] <= t['h_max'] <= t['envelope_max']
        assert t['begin'] - t['anchor'] + t['envelope_min'] >= 0
        assert t['end'] - 1 - t['anchor'] + t['envelope_max'] < 384
    edges = {}
    for edge, limit in edge_limits:
        labels = {f'jump-{edge}-still', f'land-{edge}'}
        def selected(trace):
            return [t for t in trace if t['active'] and t['camera'] == limit
                    and rows[t['completed']-1]['label'] in labels]
        current, previous = selected(camera), selected(old)
        assert len(current) >= 40, ('Missing stationary edge jump', edge, len(current))
        assert len({rows[t['completed']-1]['y'] for t in current}) > 10, 'Jump did not change height'
        assert len({t['anchor'] for t in current}) == 1, ('Horizontal bounce remains', edge)
        edges[edge] = dict(frames=len(current), stable_anchor=current[0]['anchor'],
                           previous_anchors=sorted({t['anchor'] for t in previous}),
                           h_min_values=sorted({t['h_min'] for t in current}),
                           h_max_values=sorted({t['h_max'] for t in current}))
    return dict(edges=edges, stable_keys=len(anchors), checked_frames=len(active))


def left_jump(folder, exe, arena, width):
    session = matrix.make_session(folder, exe, width, 2, rom=ROM)
    rows = []
    try:
        session.call('savestate_load', path=str(FIXTURES / f'arena-{arena:02d}/combat-ready.gbas'))
        for label, keys, count in (
            ('settle', 0x3ff, 4), ('left', 0x3df, 180), ('left-release', 0x3ff, 8),
            ('jump-left-still', 0x3bf, 30), ('land-left', 0x3ff, 60),
            ('jump-left-moving', 0x39f, 30), ('land-left-moving', 0x3ff, 60)):
            rows.extend(matrix.step(session, label, keys) for _ in range(count))
    finally:
        session.close()
        matrix.write_json(folder / 'frames.json', rows)
    assert session.process.returncode == 0
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--quick', action='store_true', help='Arena 3 / 384 probe only; not delivery proof')
    args = parser.parse_args()
    no_game_running()
    matrix.MOVEMENT = ROUTE
    out = ROOT / 'validation' / ('stable-camera-' + str(time.time_ns()))
    out.mkdir()
    protected_paths = [OLD, NEW, ROOT / 'build-native/Swordcraft3Japanese106.exe', ROM,
                       OWNER / 'gbarecomp/bios/gba_bios.bin']
    protected_paths += list(FIXTURES.glob('arena-*/*.gbas'))
    protected_paths += [ROOT / name for name in ('src/battle_follow_camera.h', 'src/custom_battle_scene.h',
                                                'src/custom_battle_state.h', 'src/custom_renderer.cpp')]
    protected = {str(p.resolve(strict=True)): file_hash(p) for p in protected_paths}
    before, main_before = snapshot(TEST), snapshot(matrix.MAIN)
    report = dict(passed=False, full_matrix=not args.quick, cases=[], transitions=[],
                  protected=protected, no_pixel_assertions=True, baseline=str(OLD), candidate=str(NEW))
    print('EVIDENCE=' + str(out), flush=True)
    try:
        arenas = [3] if args.quick else [0, 2, 3, 7, 1]
        widths = [384] if args.quick else [240, 284, 384]
        for arena in arenas:
            for width in widths:
                # Exact state hashes should match old/new within each mode:
                # no guest or extra object-submission changes in this revision.
                modes = [2] if args.quick else ([0, 1, 2] if arena == 3 else [2])
                for mode in modes:
                    base_folder = out / f'a{arena}-w{width}-m{mode}-old'
                    new_folder = out / f'a{arena}-w{width}-m{mode}-new'
                    baseline, base_result = matrix.exercise(base_folder, OLD, arena, width, mode,
                        rom=ROM, fixtures=FIXTURES)
                    rows, result = matrix.exercise(new_folder, NEW, arena, width, mode,
                        rom=ROM, fixtures=FIXTURES)
                    matrix.compare(baseline, rows, True)
                    result.update(all_recorded_guest_hashes_identical=True)
                    if mode == 2 and width > 240 and arena in matrix.SUPPORTED:
                        result['stability'] = verify_stability(new_folder, rows, base_folder)
                        # Restore before the left-edge jumps: doing them before
                        # the cross-arena route changes enemy collision timing.
                        # Do not disable/move enemies to force a test outcome.
                        old_left, new_left = out / f'a{arena}-w{width}-left-old', out / f'a{arena}-w{width}-left-new'
                        base_left = left_jump(old_left, OLD, arena, width)
                        rows_left = left_jump(new_left, NEW, arena, width)
                        matrix.compare(base_left, rows_left, True)
                        result['left_stability'] = verify_stability(new_left, rows_left, old_left, (('left', 0),))
                    report['cases'].append(result)
                    matrix.write_json(out / 'REPORT.json', report)
        if not args.quick:
            # Fresh initialization plus result/field transition; restore is
            # exercised twice in every route above. Legacy replay stays Current.
            for mode in (0, 2):
                base, _ = matrix.exercise(out / f'entry-m{mode}-old', OLD, 0, 384, mode,
                    entry=True, rom=ROM, fixtures=FIXTURES)
                rows, result = matrix.exercise(out / f'entry-m{mode}-new', NEW, 0, 384, mode,
                    entry=True, rom=ROM, fixtures=FIXTURES)
                matrix.compare(base, rows, True)
                report['cases'].append(result)
                base, _ = matrix.exercise(out / f'legacy-m{mode}-old', OLD, 3, 384, mode,
                    rom=ROM, fixtures=FIXTURES, full=False)
                rows, result = matrix.exercise(out / f'legacy-m{mode}-new', NEW, 3, 384, mode,
                    rom=ROM, fixtures=FIXTURES, full=False)
                matrix.compare(base, rows, True)
                report['cases'].append(result)
                base, _ = matrix.finish(out / f'exit-m{mode}-old', OLD, mode, rom=ROM, fixtures=FIXTURES)
                rows, result = matrix.finish(out / f'exit-m{mode}-new', NEW, mode, rom=ROM, fixtures=FIXTURES)
                matrix.compare(base, rows, True)
                report['transitions'].append(result)
        report['passed'] = True
    finally:
        report['inputs_unchanged'] = all(file_hash(Path(p)) == h for p, h in protected.items())
        report['main_unchanged'] = snapshot(matrix.MAIN) == main_before
        report['test_unchanged'] = snapshot(TEST) == before
        report['passed'] = report['passed'] and all(report[k] for k in ('inputs_unchanged', 'main_unchanged', 'test_unchanged'))
        matrix.write_json(out / 'REPORT.json', report)
    assert report['passed']
    print('PASS: stable-camera same-ROM A/B; full_matrix=' + str(report['full_matrix']), flush=True)


if __name__ == '__main__':
    main()
