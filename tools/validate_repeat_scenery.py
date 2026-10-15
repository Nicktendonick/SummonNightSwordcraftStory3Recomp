"""PRIVATE Repeat-scenery check: guest state and owned-raster counters, no pixels.

Replays one input route from a COPY of a supplied English battle save state
(forest, arena 0) in isolated folders, once per framing case. Every guest-state
hash must match Current: Repeat scenery is presentation only. Only Repeat may
serve continued scenery samples, never frame decoration, and its tile-map plans
must be built once per arena rather than every frame. Edge columns a plan
replaces (insets) must stay outside the native view on every captured scenery
row; the route must never need the inset-free fallback. The original state,
ROM, BIOS and, if given, a portable folder are hashed before and after.

No screenshot, framebuffer or pixel value is read or asserted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
ROUTE = (
    # Waits through a battle introduction (camera C=0) if the state has one.
    ('intro', 0x3ff, 270),
    ('settle', 0x3ff, 8), ('left', 0x3df, 120), ('right', 0x3ef, 420),
    ('jump-right', 0x3af, 30), ('right', 0x3ef, 60), ('left', 0x3df, 480),
    ('jump-left', 0x39f, 30), ('pause', 0x3f7, 1), ('paused', 0x3ff, 8),
    ('resume', 0x3f7, 1), ('release', 0x3ff, 12),
)
# (name, host width, battle_camera_mode). Guest state can legitimately depend
# on host width (the widened OBJ drawing cutoffs), so each Repeat case is
# compared with Current at the same width.
CASES = (('current-384', 384, 0), ('repeat-384', 384, 3),
         ('current-284', 284, 0), ('repeat-284', 284, 3),
         ('current-240', 240, 0), ('repeat-240', 240, 3))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def tree_digest(folder):
    return {str(p.relative_to(folder)): digest(p) for p in sorted(folder.rglob('*')) if p.is_file()}


def traces(log, name):
    pattern = r'\[sc3:' + re.escape(name) + r'\] (completed=[^\r\n]+)'
    return [{k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            for line in re.findall(pattern, log)]


def plans(log):
    rows = []
    for line in re.findall(r'\[sc3:battle-repeat\] ([^\r\n]+)', log):
        fields = dict(re.findall(r'(\w+)=(\S+)', line))
        rows.append(fields)
    return rows


class Session:
    def __init__(self, out, exe, rom, bios, width, mode):
        out.mkdir(parents=True)
        (out / 'Settings').mkdir()
        (out / 'Settings/battle-camera.ini').write_text(
            '[Launcher]\nbattle_camera_mode = %d\n' % mode, encoding='utf-8')
        (out / 'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 0\n', encoding='utf-8')
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(('GBARECOMP_', 'SWORDCRAFT3_', 'SDL_', 'LNG_'))}
        env['PATH'] = 'C:/msys64/mingw64/bin;' + env.get('PATH', '')
        for flag in ('CUSTOM_RENDERER', 'FULL_FIELD_RENDERER', 'FULL_COMBAT_RENDERER',
                     'CUSTOM_BATTLES', 'CUSTOM_GENERAL_FIELDS', 'CUSTOM_OBJECTS',
                     'CUSTOM_ROCKY', 'CUSTOM_ADDITIONAL_AREAS', 'STATE_TRACE'):
            env['SWORDCRAFT3_' + flag] = '1'
        env.update(SWORDCRAFT3_CUSTOM_HOST_WIDTH=str(width), SWORDCRAFT3_SELECT_GUARD='0',
                   GBARECOMP_SELFHEAL_RECOMPILE='0', SWORDCRAFT3_BETA_LAUNCHER='1',
                   SWORDCRAFT3_PORTABLE_ROOT=str(out), SDL_VIDEODRIVER='dummy',
                   SDL_AUDIODRIVER='dummy')
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            port = s.getsockname()[1]
        args = [str(exe), '--no-launcher', '--tcp', str(port), '--rom', str(rom),
                '--bios', str(bios), '--save', str(out / 'isolated.eep'),
                '--view-width', '240', str(ROOT / 'native-test.toml')]
        self.stdout = (out / 'stdout.log').open('wb')
        self.stderr = (out / 'stderr.log').open('wb')
        self.process = subprocess.Popen(args, cwd=out, env=env, stdout=self.stdout,
            stderr=self.stderr, creationflags=subprocess.CREATE_NO_WINDOW)
        self.connection = None
        deadline = time.monotonic() + 30
        while self.connection is None:
            if self.process.poll() is not None:
                raise RuntimeError('Test process exited: ' + str(out))
            try:
                self.connection = socket.create_connection(('127.0.0.1', port), timeout=1)
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(.05)
        self.connection.settimeout(60)
        self.stream = self.connection.makefile('rwb')

    def call(self, cmd, **kwargs):
        self.stream.write((json.dumps(dict(cmd=cmd, **kwargs)) + '\n').encode())
        self.stream.flush()
        result = json.loads(self.stream.readline())
        assert result.get('ok'), result
        return result

    def step(self, label, keys):
        self.call('set_keyinput', value=keys)
        frame = self.call('step')['frame']
        ram = bytes.fromhex(self.call('read_iwram', addr=0, len=0x6ac4)['data'])
        u32 = lambda n: int.from_bytes(ram[n:n+4], 'little')
        player = ram[0x8c0:0x8c0+0x390]
        hashes = self.call('state_hash')
        hashes.pop('ok', None)
        return dict(label=label, keys=keys, frame=frame, root=u32(0x6ac0), phase=u32(0x6ab4),
                    mode=ram[0xc], pause=ram[0xf], arena=ram[0x1a94],
                    camera=int.from_bytes(ram[0x1a98:0x1a9a], 'little', signed=True),
                    camera_mode=ram[0x1aa2],
                    x=int.from_bytes(player[0x188:0x18c], 'little', signed=True),
                    y=int.from_bytes(player[0x18c:0x190], 'little', signed=True),
                    hashes=hashes)

    def close(self):
        if self.connection:
            try:
                self.call('quit')
            except (OSError, ValueError, AssertionError):
                pass
            self.stream.close()
            self.connection.close()
        try:
            self.process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=10)
        self.stdout.close()
        self.stderr.close()


def run_case(out, args, state, name, width, mode):
    session = Session(out / name, args.exe, args.rom, args.bios, width, mode)
    rows = []
    try:
        session.call('savestate_load', path=str(state))
        for label, keys, count in ROUTE:
            rows.extend(session.step(label, keys) for _ in range(count))
    finally:
        session.close()
    assert session.process.returncode == 0, (name, session.process.returncode)
    (out / name / 'frames.json').write_text(json.dumps(rows) + '\n', encoding='utf-8')
    return rows, (out / name / 'stderr.log').read_text(errors='replace')


def check_case(name, width, mode, rows, log):
    frames = {t['completed']: t for t in traces(log, 'battle-repeat-frame')}
    composition = {t['completed']: t for t in traces(log, 'composition')}
    built = plans(log)
    owned = [c for c in composition.values() if c['complete_owner']]
    result = dict(case=name, width=width, mode=mode, frames=len(rows),
                  owned_frames=len(owned), plans=built,
                  cameras=sorted({r['camera'] for r in rows}))
    if width == 240:
        assert not owned and not built, 'native width must bypass the compositor'
        return result
    assert owned, 'battle never reached the full combat compositor'
    for c in owned:
        f = frames[c['completed']]
        assert c['rows'] == 160
        if mode == 3:
            assert f['active'] == 1, ('repeat inactive on an owned frame', f)
            assert c['edge_columns'] == 0 and c['edge_rows'] == 0, ('frame decoration', c)
        else:
            assert f['active'] == 0 and f['near_samples'] == 0 and f['far_samples'] == 0, f
    if mode != 3:
        assert not built, 'Current must not build repeat plans'
        return result
    arenas = {r['arena'] for r in rows}
    assert len(arenas) == 1, arenas
    arena = arenas.pop()
    near = [p for p in built if p['layer'] == 'near']
    far = [p for p in built if p['layer'] == 'far']
    # One plan per layer for one arena: entries are stable, so no re-planning.
    assert len(near) == 1 and len(far) == 1, built
    for p in near + far:
        assert p['valid'] == '1' and int(p['arena']) == arena and p['columns'] == '48'
    assert near[0]['rows'] == '20' and far[0]['rows'] == ('32' if arena == 0 else '20')
    last = max(frames.values(), key=lambda t: t['completed'])
    assert last['plans'] == 2, last
    # Insets: only the near layer's flat rows (above the ROM threshold d), at
    # most the columns no ordinary Hflat shows; never worse than the edge.
    threshold = last['threshold']
    hflat = [c + 8 + (lambda n: n // 64 if n >= 0 else -((-n + 63) // 64))((threshold - 152) * (c - 64))
             for c in range(129)]
    caps = (max(0, min(hflat)) // 8, max(0, 384 - 240 - max(hflat)) // 8)
    n = near[0]
    assert int(n['inset_rows']) == threshold // 8, (n, threshold)
    assert int(n['left_inset']) <= caps[0] and int(n['right_inset']) <= caps[1], (n, caps)
    assert int(n['left_cost']) <= int(n['left_edge_cost']), n
    assert int(n['right_cost']) <= int(n['right_edge_cost']), n
    assert far[0]['inset_rows'] == '0' and far[0]['left_inset'] == '0' and far[0]['right_inset'] == '0'
    if arena == 0:
        # Forest: the tree-base rows 12-15 had no authored left continuation.
        assert threshold == 128 and caps == (4, 4), (threshold, caps)
        assert n['left_inset'] == '4' and int(n['left_unauthored']) < int(n['left_edge_unauthored']), n
    owned_frames = [frames[c['completed']] for c in owned]
    for f in owned_frames:
        assert f['left_inset_ok'] == 1 and f['right_inset_ok'] == 1, ('inset fallback', f)
        if f['inset_lines']:
            assert f['inset_h_min'] >= f['left_inset'] * 8, f
            assert f['inset_h_max'] + 240 <= 384 - f['right_inset'] * 8, f
    shown = [f for f in owned_frames if f['inset_lines']]
    assert shown, 'no captured scenery line used an inset row'
    inset_samples = [f['inset_samples'] for f in owned_frames]
    if int(n['left_inset']) or int(n['right_inset']):
        assert max(inset_samples) > 0, 'planned insets never served'
    # Continued samples are served at both guest camera limits. Which rows
    # need them is decided by the captured scroll schedule, not by pixels.
    by_frame = {}
    for t in frames.values():
        by_frame[t['completed']] = t
    samples = [t['near_samples'] for t in frames.values() if t['active']]
    result.update(near_plan=near[0], far_plan=far[0], max_near_samples=max(samples),
                  frames_with_near_samples=sum(1 for s in samples if s),
                  frames_with_far_samples=sum(1 for t in frames.values() if t['far_samples']),
                  threshold=threshold, inset_caps=caps, max_inset_samples=max(inset_samples),
                  frames_with_inset_samples=sum(1 for s in inset_samples if s),
                  inset_h_range=[min(f['inset_h_min'] for f in shown),
                                 max(f['inset_h_max'] for f in shown)])
    assert max(samples) > 0, 'no continued near-scenery samples'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build-native/Swordcraft3Translation106.exe')
    parser.add_argument('--rom', type=Path,
        default=ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba')
    parser.add_argument('--bios', type=Path, default=ROOT / 'roms/gba_bios.bin')
    parser.add_argument('--state', type=Path, required=True,
        help='forest (arena 0) battle save state for the same ROM; only a copy is loaded')
    parser.add_argument('--portable', type=Path,
        help='optional portable folder that must stay byte-identical')
    args = parser.parse_args()
    out = ROOT / 'validation' / ('repeat-scenery-' + str(time.time_ns()))
    out.mkdir(parents=True)
    protected = [args.state, args.rom, args.bios]
    before = {str(p): digest(p) for p in protected}
    portable_before = tree_digest(args.portable) if args.portable else {}
    # The process loads a private copy; the supplied state is never opened by it.
    state = out / 'input.state'
    shutil.copyfile(args.state, state)
    report = dict(passed=False, executable_sha256=digest(args.exe), inputs=before,
                  no_framebuffer_assertions=True, cases={})
    print('Evidence: ' + str(out), flush=True)
    try:
        runs = {}
        for name, width, mode in CASES:
            rows, log = run_case(out, args, state, name, width, mode)
            runs[name] = rows
            report['cases'][name] = check_case(name, width, mode, rows, log)
            cameras = report['cases'][name]['cameras']
            print(name, 'ok', 'cameras %d..%d' % (min(cameras), max(cameras)), flush=True)
        # Presentation only: identical guest memory/cycle hashes and gameplay
        # to Current at the same host width.
        for name, width, mode in CASES:
            rows, reference = runs[name], runs['current-%d' % width]
            assert len(rows) == len(reference)
            for a, b in zip(reference, rows):
                for key in ('label', 'keys', 'root', 'phase', 'mode', 'pause', 'arena',
                            'camera', 'camera_mode', 'x', 'y', 'hashes'):
                    assert a[key] == b[key], (name, key, a['label'], a[key], b[key])
            cameras = {r['camera'] for r in rows}
            assert 0 in cameras and 128 in cameras, ('both camera limits not reached', name)
        report['guest_state_matches_current'] = True
        report['passed'] = True
    finally:
        after = {str(p): digest(p) for p in protected}
        report['inputs_unchanged'] = after == before
        report['portable_unchanged'] = (tree_digest(args.portable) if args.portable else {}) == portable_before
        (out / 'REPORT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    assert report['inputs_unchanged'] and report['portable_unchanged']
    print('PASS' if report['passed'] else 'FAIL', out / 'REPORT.json')


if __name__ == '__main__':
    main()
