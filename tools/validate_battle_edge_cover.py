"""Private edge-cover trial: unchanged gameplay/anchor, owned geometry, no pixels."""
import json
import argparse
from pathlib import Path
import time

import validate_follow_edge_camera as matrix
from validate_stable_battle_camera import ROUTE, FIXTURES, TEST, OLD, NEW, ROM
from package_alpha import file_hash
from promote_portable_beta import no_game_running
from update_guard_performance import snapshot

ROOT, OWNER = matrix.ROOT, matrix.OWNER
CAPTURE = TEST / 'Captures/20261001-145850-131-41760/frame-0000007160-1790881320908/state.gbas'


def camera_check(old_folder, folder, expected):
    old = matrix.traces((old_folder / 'stderr.log').read_text(errors='replace'), 'battle-camera')
    new_log = (folder / 'stderr.log').read_text(errors='replace')
    new = matrix.traces(new_log, 'battle-camera')
    assert len(new) == len(old)
    cover = {t['completed']: t for t in matrix.traces(new_log, 'battle-cover')}
    geometry = {}
    active = 0
    for a, b in zip(old, new):
        # All interpretation and translation stay identical; only the cover's
        # opening may shrink. Compare the actual completed-raster metadata.
        for key in ('completed', 'enabled', 'supported', 'active', 'camera', 'anchor',
                    'origin', 'width', 'camera_setting', 'h_min', 'h_max', 'source_width',
                    'object_wrap', 'h_flat', 'envelope_min', 'envelope_max'):
            assert a[key] == b[key], (key, a, b)
        c = cover[b['completed']]
        if c['active']:
            active += 1
            assert expected and c['arena'] == 3 and b['active']
            assert a['begin'] <= b['begin'] <= b['anchor']
            assert b['anchor'] + 240 <= b['end'] <= a['end']
            # A repeated horizontal key, regardless of vertical movement, must
            # yield exactly the same border and camera placement.
            current = (b['begin'], b['end'], b['anchor'])
            assert geometry.setdefault(b['h_flat'], current) == current
        else:
            assert a['begin'] == b['begin'] and a['end'] == b['end']
    assert bool(active) == expected, ('cover engagement', folder, active, expected)
    return dict(cover_frames=active, unchanged_camera=True, stable_keys=len(geometry))


def capture_run(folder, exe, cover):
    session = matrix.make_session(folder, exe, 384, 2, rom=ROM, edge_cover=cover)
    rows = []
    try:
        session.call('savestate_load', path=str(CAPTURE))
        for label, keys, count in (
            ('settle', 0x3ff, 4), ('right', 0x3ef, 30),
            ('jump-right-still', 0x3bf, 30), ('land-right', 0x3ff, 60),
            ('left', 0x3df, 720), ('jump-left-still', 0x3bf, 30), ('land-left', 0x3ff, 60)):
            rows.extend(matrix.step(session, label, keys) for _ in range(count))
    finally:
        session.close()
        matrix.write_json(folder / 'frames.json', rows)
    assert session.process.returncode == 0
    assert {0, 128} <= {r['camera'] for r in rows}
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prior-cases', type=Path, help='Recheck completed paired cases with identical protected source/binaries; rerun capture/exit')
    args = parser.parse_args()
    no_game_running()
    matrix.MOVEMENT = ROUTE
    out = ROOT / 'validation' / ('battle-edge-cover-' + str(time.time_ns()))
    out.mkdir()
    paths = [OLD, NEW, ROOT / 'build-native/Swordcraft3Japanese106.exe', ROM, CAPTURE,
             OWNER / 'gbarecomp/bios/gba_bios.bin']
    paths += list(FIXTURES.glob('arena-*/*.gbas'))
    paths += [ROOT / name for name in ('src/battle_scenery_edges.h', 'src/battle_follow_camera.h',
        'src/custom_battle_scene.h', 'src/custom_renderer.cpp', 'src/main.cpp',
        'src/battle_camera_preferences.h', 'src/combat_frame_renderer.h')]
    prior = None
    if args.prior_cases:
        args.prior_cases = args.prior_cases.resolve(strict=True)
        assert args.prior_cases.is_relative_to((ROOT / 'validation').resolve(strict=True))
        prior = json.loads(args.prior_cases.read_text())
        assert prior['kind'] == 'battle-edge-cover-v1' and len(prior['cases']) == 12
        assert all(prior[k] for k in ('inputs_unchanged', 'main_unchanged', 'test_unchanged'))
        assert all(file_hash(Path(p)) == h for p, h in prior['protected'].items())
        assert all(prior['protected'][str(p.resolve(strict=True))] == file_hash(p) for p in paths)
        paths.append(args.prior_cases)
    protected = {str(p.resolve(strict=True)): file_hash(p) for p in paths}
    before, main_before = snapshot(TEST), snapshot(matrix.MAIN)
    report = dict(kind='battle-edge-cover-v1', passed=False, protected=protected, cases=[],
                  no_pixel_assertions=True, captures=[], transitions=[])
    print('EVIDENCE=' + str(out), flush=True)
    try:
        cases = [(3, w, 2, True, False, True) for w in (240, 284, 384)]
        cases += [(3, 384, m, True, False, True) for m in (0, 1)]
        cases += [(a, 384, 2, True, False, True) for a in (0, 2, 7, 1)]
        cases += [(3, 384, 2, False, False, True), (3, 384, 2, True, True, True),
                  (3, 384, 2, True, False, False)]
        for index, (arena, width, mode, full, entry, cover) in enumerate(cases):
            case_root = args.prior_cases.parent if prior else out
            old_dir, new_dir = case_root / f'case-{index:02d}-old', case_root / f'case-{index:02d}-new'
            opts = dict(rom=ROM, fixtures=FIXTURES, full=full, entry=entry)
            if prior:
                result = dict(prior['cases'][index])
                assert result['passed'] and (result['arena'],result['width'],result['camera_mode'],result['full_renderer'],result['cover']) == (arena,width,mode,full,cover)
                base = json.loads((old_dir / 'frames.json').read_text())
                rows = json.loads((new_dir / 'frames.json').read_text())
                for folder, frames in ((old_dir, base), (new_dir, rows)):
                    matrix.inspect_trace((folder / 'stderr.log').read_text(errors='replace'), arena, width,
                        mode, [result.get('initializer_entry_frames',0)+1,len(frames)-3], full)
                result['rechecked_prior_directory'] = str(case_root)
            else:
                base, _ = matrix.exercise(old_dir, OLD, arena, width, mode, edge_cover=False, **opts)
                rows, result = matrix.exercise(new_dir, NEW, arena, width, mode, edge_cover=cover, **opts)
            matrix.compare(base, rows, True)
            expected = cover and full and arena == 3 and mode == 2 and width > 240
            result.update(camera_check(old_dir, new_dir, expected), exact_guest_hashes=True, cover=cover)
            report['cases'].append(result)
            matrix.write_json(out / 'REPORT.json', report)
        base = capture_run(out / 'player-capture-old', OLD, False)
        rows = capture_run(out / 'player-capture-new', NEW, True)
        matrix.compare(base, rows, True)
        result = camera_check(out / 'player-capture-old', out / 'player-capture-new', True)
        result.update(frames=len(rows), exact_guest_hashes=True, path=str(CAPTURE))
        report['captures'].append(result)
        # Positive rocky-arena battle -> results -> field return: the cover
        # must retire with its owner, without altering the guest's outcome.
        base, _ = matrix.finish(out / 'exit-old', OLD, 2, rom=ROM, fixtures=FIXTURES, edge_cover=False, arena=3)
        rows, result = matrix.finish(out / 'exit-new', NEW, 2, rom=ROM, fixtures=FIXTURES, edge_cover=True, arena=3)
        matrix.compare(base, rows, True)
        result.update(camera_check(out / 'exit-old', out / 'exit-new', True), exact_guest_hashes=True)
        report['transitions'].append(result)
        report['passed'] = True
    finally:
        report['inputs_unchanged'] = all(file_hash(Path(p)) == h for p, h in protected.items())
        report['main_unchanged'] = snapshot(matrix.MAIN) == main_before
        report['test_unchanged'] = snapshot(TEST) == before
        report['passed'] &= all(report[k] for k in ('inputs_unchanged', 'main_unchanged', 'test_unchanged'))
        matrix.write_json(out / 'REPORT.json', report)
    assert report['passed']
    print('PASS: edge-cover geometry/guest/camera matrix; ' + str(out / 'REPORT.json'), flush=True)


if __name__ == '__main__':
    main()
