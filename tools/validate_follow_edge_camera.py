"""PRIVATE state/input matrix for Follow + edge stops; never inspect pixels.

Run alone after both language engines build. Historical Current/Bounded are
compared with the installed old engine; the new mode permits OBJ submission
bookkeeping only. Fixtures, ROM, BIOS, binaries and Portable Beta stay intact.
"""
import argparse
import json
from pathlib import Path
import re
import time

from audit_field_provenance import field_state
from package_alpha import file_hash
from promote_portable_beta import no_game_running
from update_guard_performance import snapshot
from validate_battle_camera import FIXTURES
from validate_guard_experiment import Session
from validate_translation105 import ROOT, OWNER, ROM

MAIN = OWNER / 'release/Portable Beta'
OLD = MAIN / 'Runtime/Swordcraft3CustomRendererBeta.exe'
NEW = ROOT / 'build-native/Swordcraft3Translation106.exe'
NEW_ROM = ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
SUPPORTED = (0, 2, 3, 7)
MOVEMENT = (
    ('settle', 0x3ff, 4), ('left', 0x3df, 180), ('guard', 0x3fb, 12),
    ('guard-release', 0x3ff, 8), ('right', 0x3ef, 240),
    ('jump-right', 0x3af, 30), ('right', 0x3ef, 100),
    ('pause', 0x3f7, 1), ('paused', 0x3ff, 8),
    ('resume', 0x3f7, 1), ('release', 0x3ff, 12),
)
GAMEPLAY = ('label', 'keys', 'root', 'phase', 'mode', 'pause', 'auto', 'arena',
            'slot', 'effect_kind', 'effect_stage', 'flags', 'guard', 'timer',
            'animation', 'player', 'held', 'new', 'camera', 'camera_mode', 'x', 'y')


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def traces(log, name):
    return [{k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            for line in re.findall(r'\[sc3:' + re.escape(name) + r'\] (completed=[^\r\n]+)', log)]


def step(session, label, keys=0x3ff):
    row = session.step(keys)
    ram = bytes.fromhex(session.call('read_iwram', addr=0x1a98, len=12)['data'])
    player = bytes.fromhex(row['player'])
    row.update(label=label, camera=int.from_bytes(ram[:2], 'little', signed=True),
               camera_mode=ram[10], x=int.from_bytes(player[0x188:0x18c], 'little', signed=True),
               y=int.from_bytes(player[0x18c:0x190], 'little', signed=True))
    return row


def make_session(out, exe, width, mode, old=False, guard=True, rom=ROM, full=True, edge_cover=None):
    options = {'camera': bool(mode)} if old else {'camera_mode': mode}
    return Session(out, guard, exe, rom, edge_cover=edge_cover, diagnostic_env={
        'SWORDCRAFT3_CUSTOM_HOST_WIDTH': str(width),
        'SWORDCRAFT3_FULL_COMBAT_RENDERER': '1' if full else '0',
        'SWORDCRAFT3_STATE_TRACE': '1', 'SWORDCRAFT3_GUARD_TRACE': '0'}, **options)


def inspect_trace(log, arena, width, mode, restore_completed, full=True):
    state = traces(log, 'state-frame')
    camera = traces(log, 'battle-camera')
    composition = {t['completed']: t for t in traces(log, 'composition')}
    active = [t for t in camera if t['active']]
    expected = full and width > 240 and arena in SUPPORTED and mode != 0
    assert bool(active) == expected, (arena, width, mode, 'framing engagement', len(active))
    assert state, 'Missing state hook observations'
    if width == 240:
        assert not composition and all(t['wide'] == 0 for t in state)
    if arena not in SUPPORTED:
        assert all(t['active'] == 0 for t in state), 'Unsupported arena acquired wide owner'
    if expected:
        assert any(t['camera'] == 0 for t in active), 'Left guest camera limit not reached'
        assert any(t['camera'] == 128 for t in active), 'Right guest camera limit not reached'
    for t in active:
        assert 0 <= t['begin'] < t['end'] <= width
        assert t['begin'] <= t['anchor'] and t['anchor'] + 240 <= t['end']
        if mode == 1:
            shown = min(width, 368)
            assert t['begin'] == (width - shown) // 2 and t['end'] == t['begin'] + shown
            assert 0 <= t['origin'] <= 368 - shown
            assert t['begin'] - t['anchor'] + t['camera'] == t['origin']
        else:
            # These are authored source-coordinate assertions, not sampled
            # colors or framebuffer assertions. The renderer reports the
            # authenticated completed-raster extrema it actually consumed.
            assert t['h_min'] <= t['h_max'] and t['source_width'] == 384
            assert t['begin'] - t['anchor'] + t['h_min'] >= 0
            assert t['end'] - 1 - t['anchor'] + t['h_max'] < t['source_width']
        c = composition[t['completed']]
        assert c['complete_owner'] and c['rows'] == 160
        pad = t['begin'] + width - t['end']
        assert c['edge_columns'] == pad * c['edge_rows']
        assert 0 <= c['edge_rows'] <= 125
        if pad:
            assert c['edge_rows'] > 0, 'Bounded source edges were not decorated'
    # Each explicit restore starts with the exact same invalidation behavior;
    # no old camera anchor is permitted before a fresh owned raster exists.
    by_completed = {t['completed']: t for t in state}
    for completed in restore_completed:
        assert by_completed[completed]['active'] == 0, ('Retained battle owner after restore', completed)
        assert not any(t['completed'] == completed and t['active'] for t in camera)
    return dict(active_frames=len(active), native_frames=sum(t['wide'] == 0 for t in state),
                anchors=sorted({t['anchor'] for t in active}),
                guest_cameras=sorted({t['camera'] for t in active}),
                restored_first_frame_rejected=len(restore_completed), no_framebuffer_assertions=True)


def exercise(out, exe, arena, width, mode, old=False, entry=False, rom=ROM, fixtures=FIXTURES, full=True, edge_cover=None):
    ready = fixtures / f'arena-{arena:02d}/combat-ready.gbas'
    fixture = fixtures / f'arena-{arena:02d}/input.gbas'
    session = make_session(out, exe, width, mode, old, rom=rom, full=full, edge_cover=edge_cover)
    rows, restored = [], []
    result = dict(passed=False, arena=arena, width=width, camera_mode=mode, old_engine=old, full_renderer=full)
    try:
        if entry:
            session.call('savestate_load', path=str(fixture))
            for _ in range(480):
                row = step(session, 'entry'); rows.append(row)
                if row['root'] == 0x03000000 and row['phase'] == 4 and row['mode'] == 2:
                    assert row['arena'] == arena
                    break
            else:
                raise AssertionError('Fresh initializer entry did not reach owned combat')
            result['initializer_entry_frames'] = len(rows)
        session.call('savestate_load', path=str(ready))
        restored.append(len(rows) + 1)
        start = len(rows)
        for label, keys, count in MOVEMENT:
            for _ in range(count):
                rows.append(step(session, label, keys))
        movement = rows[start:]
        assert any(r['guard'] for r in movement if r['label'] == 'guard'), 'Guard never engaged'
        assert not any(r['guard'] for r in movement if r['label'] in ('guard-release', 'release'))
        assert all(r['auto'] == 0 for r in movement), 'Select changed native auto-battle'
        assert any(r['pause'] for r in movement if r['label'] == 'paused'), 'Pause never engaged'
        # Native START closes with a four-frame animation (pause substate 3).
        # Require settled resume, and compare the entire transition old/new.
        released = [r for r in movement if r['label'] == 'release']
        assert all(r['pause'] == 0 and r['mode'] == 2 for r in released[-4:]), 'Pause did not release'
        assert len({r['y'] for r in movement if r['label'] == 'jump-right'}) > 1, 'Jump did not change height'
        assert len({r['x'] for r in movement}) > 2, 'Movement did not change position'
        # Repeat exactly the initial neutral inputs after restoring from the
        # opposite endpoint. This checks guest reset and captures host reset.
        session.call('savestate_load', path=str(ready))
        restored.append(len(rows) + 1)
        restored_rows = [step(session, 'settle') for _ in range(4)]
        assert [{k: r[k] for k in GAMEPLAY} for r in restored_rows] == [
            {k: r[k] for k in GAMEPLAY} for r in movement[:4]], 'Restore gameplay differed'
        rows.extend(restored_rows)
    finally:
        session.close()
        write_json(out / 'frames.json', rows)
    log = (out / 'stderr.log').read_text(errors='replace')
    result.update(inspect_trace(log, arena, width, mode, restored, full))
    assert session.process.returncode == 0
    assert len(traces(log, 'state-frame')) == len(rows), 'Unexpected asynchronous frame progression'
    result.update(passed=True, frames=len(rows), clean_exit=True)
    write_json(out / 'summary.json', result)
    print(json.dumps(result), flush=True)
    return rows, result


def compare(base, candidate, exact):
    keys = GAMEPLAY + (('hashes',) if exact else ())
    assert len(base) == len(candidate), 'Input route lengths diverged'
    for n, (a, b) in enumerate(zip(base, candidate)):
        for key in keys:
            assert a[key] == b[key], ('Guest-state regression', n, key, a[key], b[key])


def finish(out, exe, mode, old=False, rom=ROM, fixtures=FIXTURES, edge_cover=None, arena=0):
    session = make_session(out, exe, 384, mode, old, guard=False, rom=rom, edge_cover=edge_cover)
    rows = []
    reached_result = returned_field = False
    try:
        session.call('savestate_load', path=str(fixtures / f'arena-{arena:02d}/combat-ready.gbas'))
        session.call('run_frames', n=2, keyinput=0x3fb)
        for n in range(160):
            session.call('run_frames', n=29, keyinput=0x3ff)
            row = step(session, 'result-route', 0x3fe if reached_result and n % 2 == 0 else 0x3ff)
            rows.append(row)
            if row['root'] == 0x03000000 and row['phase'] in (5, 6, 7, 8, 9):
                reached_result = True
            if reached_result:
                iwram = bytes.fromhex(session.call('read_iwram', addr=0, len=0x8000)['data'])
                ewram = bytes.fromhex(session.call('read_ewram', addr=0, len=0x40000)['data'])
                if field_state(ewram, iwram).get('owned'):
                    returned_field = True
                    for _ in range(4): rows.append(step(session, 'returned-field'))
                    break
    finally:
        session.close()
        write_json(out / 'frames.json', rows)
    assert reached_result and returned_field and session.process.returncode == 0
    log = (out / 'stderr.log').read_text(errors='replace')
    assert '[sc3:state-exit] pc=0805e780 reason=result-panel' in log
    state = traces(log, 'state-frame')
    assert all(t['active'] == 0 for t in state[-4:]), 'Retained combat owner after field return'
    result = dict(passed=True, result_reached=True, field_returned=True, camera_mode=mode,
                  old_engine=old, observations=len(rows), clean_exit=True)
    write_json(out / 'summary.json', result)
    print(json.dumps(result), flush=True)
    return rows, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old-exe', type=Path, default=OLD)
    parser.add_argument('--new-exe', type=Path, default=NEW)
    parser.add_argument('--new-rom', type=Path, default=NEW_ROM)
    parser.add_argument('--new-fixtures', type=Path, required=True,
                        help='Private, separately audited fixtures bound to the new ROM; never user save states')
    parser.add_argument('--arenas', type=int, nargs='+', choices=(*SUPPORTED, 1), default=[*SUPPORTED, 1])
    parser.add_argument('--widths', type=int, nargs='+', choices=(240, 284, 384), default=[240, 284, 384])
    parser.add_argument('--skip-finish', action='store_true', help='Quick probe only; report is not a full transition matrix')
    args = parser.parse_args()
    no_game_running()
    out = ROOT / 'validation' / ('follow-edge-' + str(time.time_ns()))
    out.mkdir()
    protected_paths = [args.old_exe, args.new_exe, ROM, args.new_rom, OWNER / 'gbarecomp/bios/gba_bios.bin']
    for arena in args.arenas:
        protected_paths.extend(FIXTURES / f'arena-{arena:02d}' / name for name in ('input.gbas', 'combat-ready.gbas'))
        protected_paths.extend(args.new_fixtures / f'arena-{arena:02d}' / name for name in ('input.gbas', 'combat-ready.gbas'))
    if not args.skip_finish:
        protected_paths.append(FIXTURES / 'arena-00/combat-ready.gbas')
        protected_paths.append(args.new_fixtures / 'arena-00/combat-ready.gbas')
    protected = {str(p.resolve(strict=True)): file_hash(p) for p in protected_paths}
    before = snapshot(MAIN)
    report = dict(passed=False, candidate=str(args.new_exe.resolve()), candidate_sha256=file_hash(args.new_exe),
                  source_hashes=protected, candidate_rom=str(args.new_rom.resolve()),
                  candidate_rom_sha256=file_hash(args.new_rom), diagnostic_fixtures=str(args.new_fixtures.resolve()),
                  cases=[], transitions=[], no_framebuffer_assertions=True,
                  full_matrix=set(args.arenas) == {*SUPPORTED, 1} and set(args.widths) == {240, 284, 384} and not args.skip_finish)
    print('EVIDENCE=' + str(out), flush=True)
    try:
        for arena in args.arenas:
            for width in args.widths:
                entry = arena == 0 and width == 384
                old, result = exercise(out / f'a{arena}-w{width}-old-current', args.old_exe, arena, width, 0, True, entry)
                report['cases'].append(result)
                current, result = exercise(out / f'a{arena}-w{width}-new-current', args.new_exe, arena, width, 0, entry=entry,
                                           rom=args.new_rom, fixtures=args.new_fixtures)
                compare(old, current, True)
                result['old_guest_hashes_identical'] = True
                report['cases'].append(result)
                follow, result = exercise(out / f'a{arena}-w{width}-new-follow', args.new_exe, arena, width, 2, entry=entry,
                                          rom=args.new_rom, fixtures=args.new_fixtures)
                exact = width == 240 or arena not in SUPPORTED
                compare(old, follow, exact)
                result.update(gameplay_matches_current=True, exact_fallback_hashes=exact)
                report['cases'].append(result)
                if arena == 0:
                    old_bounded, result = exercise(out / f'a{arena}-w{width}-old-bounded', args.old_exe, arena, width, 1, True)
                    report['cases'].append(result)
                    bounded, result = exercise(out / f'a{arena}-w{width}-new-bounded', args.new_exe, arena, width, 1,
                                               rom=args.new_rom, fixtures=args.new_fixtures)
                    compare(old_bounded, bounded, True)
                    result['old_guest_hashes_identical'] = True
                    report['cases'].append(result)
                write_json(out / 'REPORT.json', report)
        if not args.skip_finish:
            # Follow must not widen its OAM union without the compositor that
            # decodes it. Legacy raster replay remains exactly Current.
            legacy, result = exercise(out / 'legacy-old-current', args.old_exe, 0, 384, 0, True, full=False)
            report['cases'].append(result)
            for mode in (0, 2):
                rows, result = exercise(out / f'legacy-new-mode{mode}', args.new_exe, 0, 384, mode,
                    rom=args.new_rom, fixtures=args.new_fixtures, full=False)
                compare(legacy, rows, True)
                result['old_guest_hashes_identical'] = True
                report['cases'].append(result)
            baseline, result = finish(out / 'old-current-exit', args.old_exe, 0, True)
            report['transitions'].append(result)
            for mode in (0, 2):
                rows, result = finish(out / f'new-mode{mode}-exit', args.new_exe, mode,
                                      rom=args.new_rom, fixtures=args.new_fixtures)
                compare(baseline, rows, mode == 0)
                result['gameplay_matches_current'] = True
                report['transitions'].append(result)
        report['passed'] = True
    finally:
        report['inputs_unchanged'] = all(file_hash(Path(p)) == h for p, h in protected.items())
        report['main_unchanged'] = snapshot(MAIN) == before
        report['passed'] = report['passed'] and report['inputs_unchanged'] and report['main_unchanged']
        write_json(out / 'REPORT.json', report)
        assert report['inputs_unchanged'] and report['main_unchanged']
    print('PASS: state/input/source-coordinate matrix; full_matrix=' + str(report['full_matrix']), flush=True)


if __name__ == '__main__':
    main()
