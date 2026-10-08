"""Private F10 export regression: state/file evidence, never pixel assertions.

Uses the real runtime export through its deterministic Assist script and the
separate SDL input regression for physical F10 routing. No portable is rebuilt
or installed. Every execution gets its own settings, save and capture folder.
"""
import json
import re
from pathlib import Path
import subprocess

from benchmark_presentation_filters import environment
from package_alpha import file_hash
from update_guard_performance import ROOT, OWNER, snapshot, stamp

EXE = ROOT / 'build-native/Swordcraft3Translation106.exe'
ROM = ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
BIOS = OWNER / 'gbarecomp/bios/gba_bios.bin'


def execute(out, name, script='', directory=None, debugger=None, state=None):
    run = out / name
    run.mkdir()
    (run / 'Settings').mkdir()
    (run / 'Settings/battle-camera.ini').write_text('[Launcher]\nbattle_camera_mode = 2\n')
    neutral = run / 'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n')
    env = environment(run, 384, neutral, audit=True)
    env['GBARECOMP_STRICT_STATIC'] = '1'
    env['GBARECOMP_STATE_TRACE'] = str(run / 'state.jsonl')
    if script:
        env['GBARECOMP_ASSIST_SCRIPT'] = script
    if debugger is not None:
        env['GBARECOMP_VISIBLE_DEBUGGER'] = debugger
    capture = run / (directory or 'debug-captures')
    if directory:
        env['GBARECOMP_DEBUG_CAPTURE_DIR'] = str(capture)
    if name == 'unwritable':
        capture.write_text('An existing file, deliberately not a directory.\n')
    command = [str(EXE), '--window', '--no-launcher', '--frames', '90',
               '--view-width', '240', '--rom', str(ROM), '--bios', str(BIOS),
               '--save', str(run / 'private.eep')]
    if state:
        command += ['--load-state', str(state)]
    command.append(str(ROOT / 'native-test.toml'))
    with (run / 'stdout.log').open('wb') as stdout, (run / 'stderr.log').open('wb') as stderr:
        child = subprocess.Popen(command, cwd=run, env=env, stdout=stdout, stderr=stderr,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=60)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
    assert child.returncode == 0, name + ': inspect logs'
    log = (run / 'stdout.log').read_text(errors='replace')
    errors = (run / 'stderr.log').read_text(errors='replace')
    if name == 'paused':
        # Actions queue their pause change until the end of the input pump;
        # observe the next pump, not the pre-application action log.
        assert re.search(r'event=menu_probe pump=13[^\n]+manual_pause=1', errors)
        assert re.search(r'event=menu_probe pump=21[^\n]+manual_pause=0', errors)
    assert 'dispatch_misses=0' in log
    if state:
        assert 'savestate_loaded' in log
    records = []
    for path in sorted(capture.glob('frame-*/report.json')) if capture.is_dir() else []:
        record = json.loads(path.read_text())
        assert record['frame_saved'] and record['state_saved']
        assert record['host_width'] == 384 and record['layer_mask'] == 31
        assert (path.parent / 'frame.png').stat().st_size > 32
        assert (path.parent / 'state.gbas').stat().st_size > 1024
        records.append(dict(path=str(path), **record))
    trace = [json.loads(line) for line in (run / 'state.jsonl').read_text().splitlines()]
    assert len(trace) >= 85, (name, len(trace))
    return dict(name=name, clean_exit=True, captures=records,
                no_dispatch_misses=True, frames=len(trace)), trace, errors


def main():
    out = ROOT / 'validation' / ('f10-capture-' + stamp())
    out.mkdir()
    protected = {str(p): file_hash(p) for p in (ROM, BIOS,
        OWNER / 'release/Portable Camera Edge Test/Runtime/Swordcraft3CustomRendererBeta.exe',
        OWNER / 'release/Portable Camera Edge Test/Runtime/Swordcraft3Japanese.exe')}
    main_before = snapshot(OWNER / 'release/Portable Beta')
    result = dict(passed=False, engine=str(EXE), engine_sha256=file_hash(EXE),
                  private_only=True, portable_installed=False, cases=[])
    print('Evidence: ' + str(out), flush=True)
    try:
        base, baseline, _ = execute(out, 'baseline')
        result['cases'].append(base)
        for name, directory, debugger, expected in (
            ('disabled', None, None, 0),
            ('portable', 'Captures/session with spaces', None, 2),
            ('debugger-off', 'Captures/session', '0', 2),
            ('debugger', None, '1', 2),
            ('unwritable', 'blocked', None, 0),
        ):
            case, trace, errors = execute(out, name, '20:capture;40:capture', directory, debugger)
            assert len(case['captures']) == expected, (name, case)
            assert trace == baseline, name + ': capture changed guest state'
            if name == 'unwritable':
                assert 'debugger: cannot create' in errors
            case['guest_state_unchanged'] = True
            result['cases'].append(case)
            print('PASS: ' + name, flush=True)
        case, trace, _ = execute(out, 'paused',
            '10:menu_open;12:menu_pause;13:menu_probe;15:capture;20:menu_resume;21:menu_probe;30:capture', 'Captures/session')
        assert len(case['captures']) == 2
        assert trace == baseline, 'Paused capture changed deterministic guest progression'
        case['guest_state_unchanged'] = True
        result['cases'].append(case)
        saved = Path(result['cases'][2]['captures'][0]['path']).parent / 'state.gbas'
        case, _, _ = execute(out, 'restore-capture', state=saved)
        result['cases'].append(case)
        assert all(file_hash(Path(path)) == digest for path, digest in protected.items())
        assert snapshot(OWNER / 'release/Portable Beta') == main_before
        result.update(passed=True, main_unchanged=True, inputs_and_test_engines_unchanged=True,
                      no_pixel_assertions=True, snapshot_roundtrip=True)
    finally:
        (out / 'REPORT.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: F10 exporter, portable/debugger gates, repeat capture, pause, restore, unchanged guest state', flush=True)


if __name__ == '__main__':
    main()
