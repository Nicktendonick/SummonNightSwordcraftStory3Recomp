"""Isolated windowed presentation-filter benchmark; never reads rendered pixels.

Runs the existing executable without duplicating it or its DLLs. Every run uses
its own portable settings/save directory, identical read-only private ROM/BIOS/
combat-state inputs, and neutral replay. The default 180 frames (60 warm-up) is
a short smoke benchmark; use --frames 600 --passes 2 for a steadier comparison.
--audit-state adds separate short dummy-window state/ownership checks, excluded
from timings. No screenshots, framebuffers, player settings, or original saves
are modified. Run alone: this tool owns its child windows, never other games.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OWNER = ROOT.parents[1]
DEFAULT_STATE = OWNER / 'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000061510-1788964715395/state.gbas'
MODES = (
    ('nearest-off', 0, 0), ('sharp-off', 2, 0), ('smooth-off', 3, 0),
    ('nearest-lcd', 0, 1), ('nearest-crt', 0, 2), ('smooth-crt', 3, 2),
)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def values(line):
    return {key: float(value) if '.' in value else int(value)
            for key, value in re.findall(r'(\w+)=([\d.]+)', line)}


def cadence(path, warmup):
    with path.open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) >= warmup + 30, 'Insufficient presented frames after warm-up'
    selected = rows[warmup:]
    gaps = [int(row['gap_us']) / 1000 for row in selected if int(row['gap_us']) > 0]
    assert len(gaps) >= 30 and sum(gaps) > 0, 'No positive present intervals'
    ordered = sorted(gaps)
    percentile = lambda fraction: ordered[int((len(ordered) - 1) * fraction)]
    return dict(presents=len(rows), measured_intervals=len(gaps),
                measured_seconds=sum(gaps) / 1000, fps=1000 / statistics.mean(gaps),
                gap_ms_p50=percentile(.50), gap_ms_p95=percentile(.95),
                gap_ms_p99=percentile(.99), longest_gap_ms=max(gaps),
                gaps_over_25ms=sum(gap > 25 for gap in gaps),
                gaps_over_33_5ms=sum(gap > 33.5 for gap in gaps),
                fullscreen_samples=sum(int(row['fullscreen']) for row in rows))


def environment(run, width, neutral, audit=False):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('GBARECOMP_', 'SWORDCRAFT3_', 'SDL_', 'LNG_'))}
    env['PATH'] = 'C:\\msys64\\mingw64\\bin;' + env.get('PATH', '')
    for flag in ('CUSTOM_RENDERER', 'FULL_FIELD_RENDERER', 'FULL_COMBAT_RENDERER',
                 'CUSTOM_BATTLES', 'CUSTOM_GENERAL_FIELDS', 'CUSTOM_OBJECTS',
                 'CUSTOM_ROCKY', 'CUSTOM_ADDITIONAL_AREAS', 'BATTLE_HUD_BORDERS'):
        env['SWORDCRAFT3_' + flag] = '1'
    env.update(SWORDCRAFT3_CUSTOM_HOST_WIDTH=str(width),
               SWORDCRAFT3_CUSTOM_REPLAY_CHECK='0', SWORDCRAFT3_BETA_LAUNCHER='1',
               SWORDCRAFT3_PORTABLE_ROOT=str(run), GBARECOMP_SELFHEAL_RECOMPILE='0',
               GBARECOMP_PHASE_PROF='0', GBARECOMP_INPUT_REPLAY=str(neutral),
               GBARECOMP_INPUT_REPLAY_EXCLUSIVE='1')
    if audit:
        env.update(SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy',
                   SWORDCRAFT3_STATE_TRACE='1', SWORDCRAFT3_CUSTOM_AUDIT='1')
    else:
        env.update(GBARECOMP_PRESENT_CADENCE='1',
                   GBARECOMP_PRESENT_CADENCE_DUMP=str(run / 'cadence.csv'),
                   GBARECOMP_FRAME_PHASE=str(run / 'frame-phase.csv'),
                   GBARECOMP_AUDIO_PROBE='1')
    return env


def execute(args, run, width, mode, neutral, audit=False):
    name, scaling, effect = mode
    run.mkdir()
    (run / 'Settings').mkdir()
    frames = 90 if audit else args.frames
    command = [str(args.exe), '--window', '--no-launcher', '--scale', '3',
               '--frames', str(frames), '--screen', 'raw', '--view-width', '240',
               '--linear-filter', str(int(scaling == 1)),
               '--sharp-filter', str(int(scaling == 2)),
               '--smooth-filter', str(int(scaling == 3)),
               '--screen-effect', ('off', 'lcd', 'crt')[effect], '--screen-effect-strength', str(args.strength),
               '--bios', str(args.bios), '--rom', str(args.rom),
               '--save', str(run / 'private.eep'), '--load-state', str(args.state),
               str(ROOT / 'native-test.toml')]
    started = time.perf_counter()
    with (run / 'stdout.log').open('wb') as stdout, (run / 'stderr.log').open('wb') as stderr:
        child = subprocess.Popen(command, cwd=run, env=environment(run, width, neutral, audit),
                                 stdout=stdout, stderr=stderr,
                                 creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        try:
            child.wait(timeout=max(45, frames / 20 + 30))
            assert child.returncode == 0, f'Child exit {child.returncode}; inspect {run}'
        finally:
            if child.poll() is None:
                # Only this test's child is eligible for timeout cleanup.
                child.terminate()
                child.wait(timeout=15)
    elapsed = time.perf_counter() - started
    stdout = (run / 'stdout.log').read_text(errors='replace')
    stderr = (run / 'stderr.log').read_text(errors='replace')
    assert 'savestate_loaded' in stdout, f'State was not restored: {run}'
    assert elapsed > 0
    summaries = re.findall(r'\[presentation-summary\] ([^\r\n]+)', stderr)
    assert len(summaries) == 1, f'Missing/ambiguous presentation summary: {run}'
    summary = values(summaries[0])
    assert summary['frames'] >= frames - 2 and summary['frames'] > 0, (run, summary)
    assert summary['fallback'] == 0, f'Presentation filter fell back: {run}: {summary}'
    # At exact integer 3x, Sharp intentionally uses the direct nearest path.
    # Fractional-resize engagement is covered by the separate input/UI test;
    # requiring a sharp prescale here would incorrectly reject this fast path.
    for key, enabled in (('sharp', False), ('smooth', scaling == 3),
                         ('effect', effect != 0 and args.strength > 0)):
        assert (summary[key] > 0) == enabled, f'{key} engagement mismatch: {run}: {summary}'
    result = dict(width=width, mode=name, scaling=scaling, effect=effect,
                  strength=args.strength, frames_requested=frames, elapsed_seconds=elapsed,
                  presentation=summary, sharp_fractional_path_expected=False,
                  log_directory=str(run))
    if audit:
        hooks = [values(line) for line in re.findall(r'\[sc3:state-hook\] ([^\r\n]+)', stderr)]
        assert any(row.get('supported') == 1 for row in hooks), 'Verified battle hook unavailable'
        assert any(row.get('call', 0) > 0 and row.get('enabled') == 1 for row in hooks), 'No active battle-state calls'
        compositions = [values(line) for line in re.findall(r'\[sc3:composition\] ([^\r\n]+)', stderr)]
        owned = [row for row in compositions if row.get('complete_owner') == 1]
        if width == 384:
            assert owned, 'No complete-frame wide combat ownership in audit'
            assert all(row['rows'] == 160 and row['center'] == 240 * 160 and
                       row['extended'] == (384 - 240) * 160 for row in owned)
        else:
            assert not owned, 'Native case unexpectedly widened combat'
        result.update(audit=True, battle_hook_calls=sum('call' in row for row in hooks),
                      complete_combat_frames=len(owned))
        return result
    assert '[gba-audio-probe]' in stderr, 'No real-audio processing evidence'
    assert '[present-cadence]' in stderr, 'No present-cadence evidence'
    result.update(cadence(run / 'cadence.csv', args.warmup_frames))
    assert result['presents'] >= frames - 2, 'Early presentation stop'
    if width == 384:
        battle_counts = re.findall(r'\[sc3:battle\] frames=(\d+)', stderr)
        assert battle_counts and max(map(int, battle_counts)) > 0, 'No wide combat frames'
    result['diagnostics'] = [line for line in stderr.splitlines() if any(tag in line for tag in
        ('host_window:', '[presentation]', '[presentation-summary]', '[gba-audio-probe]',
         '[sc3:custom]', '[sc3:host]', '[sc3:battle]', '[present-cadence]'))]
    return result


def write_reports(out, report):
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    rows = ['# Presentation filter benchmark', '',
            'Windowed SDL present cadence with real audio; no visual/framebuffer assertions.', '',
            f'Frames per run: {report["frames"]}; excluded warm-up frames: {report["warmup_frames"]}.', '',
            '| Width | Filter | Pass | FPS | p95 gap (ms) | p99 gap (ms) | Fallback frames |',
            '| --- | --- | --- | ---: | ---: | ---: | ---: |']
    for run in report['runs']:
        rows.append(f'| {run["width"]} | {run["mode"]} | {run["pass"]} | '
                    f'{run["fps"]:.2f} | {run["gap_ms_p95"]:.2f} | {run["gap_ms_p99"]:.2f} | '
                    f'{run["presentation"]["fallback"]} |')
    rows += ['', 'Sharp uses its direct nearest path at exact integer 3x; fractional sharp engagement is tested separately.',
             'Short runs are smoke measurements, not a promise for every scene or computer.',
             'State audits, when requested, are separate and excluded from timing.',
             f'Private source inputs unchanged: {report.get("inputs_unchanged", "not yet checked")}.', '']
    (out / 'report.md').write_text('\n'.join(rows), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build-native/Swordcraft3CustomRendererBeta.exe')
    parser.add_argument('--rom', type=Path, default=OWNER / 'build-beta/rom-patch-cache/swordcraft3_beta.gba')
    parser.add_argument('--bios', type=Path, default=OWNER / 'gbarecomp/bios/gba_bios.bin')
    parser.add_argument('--state', type=Path, default=DEFAULT_STATE)
    parser.add_argument('--frames', type=int, default=180)
    parser.add_argument('--warmup-frames', type=int, default=60)
    parser.add_argument('--widths', type=int, nargs='+', choices=(240, 384), default=[240, 384])
    parser.add_argument('--modes', nargs='+', choices=[mode[0] for mode in MODES])
    parser.add_argument('--passes', type=int, choices=(1, 2), default=1)
    parser.add_argument('--strength', type=int, default=35)
    parser.add_argument('--audit-state', action='store_true')
    args = parser.parse_args()
    assert 90 <= args.frames <= 3600 and 0 <= args.warmup_frames <= args.frames - 30
    assert 1 <= args.strength <= 100
    out = args.output.resolve()
    assert out.is_relative_to(ROOT / 'validation') and not out.exists(), 'Use a fresh validation directory'
    for name in ('exe', 'rom', 'bios', 'state'):
        setattr(args, name, getattr(args, name).resolve())
        assert getattr(args, name).is_file(), f'Missing {name}: {getattr(args, name)}'
    fingerprints = {name: digest(getattr(args, name)) for name in ('exe', 'rom', 'bios', 'state')}
    # This fixture and executable are the English instruction corpus only.
    assert hashlib.sha1(args.rom.read_bytes()).hexdigest() == 'bb2eebf98deb59bb6218442c2308bb5033ae2915'
    out.mkdir(parents=True)
    neutral = out / 'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
    report = dict(method='windowed SDL presents, real audio, integer 3x window, raw colors, neutral input',
                  frames=args.frames, warmup_frames=args.warmup_frames,
                  source_sha256=fingerprints, state=str(args.state), audits=[], runs=[])
    modes = [mode for mode in MODES if not args.modes or mode[0] in args.modes]
    try:
        if args.audit_state:
            for width in args.widths:
                print(f'State/ownership audit at {width} (excluded from timings)', flush=True)
                report['audits'].append(execute(args, out / f'audit-{width}', width, MODES[0], neutral, True))
                write_reports(out, report)
        for pass_number in range(args.passes):
            # Reverse repeat order to expose some warm-up/ordering bias.
            ordered = modes if pass_number == 0 else list(reversed(modes))
            for width in args.widths:
                for mode in ordered:
                    print(f'Benchmark {width} {mode[0]} pass {pass_number + 1}', flush=True)
                    run = execute(args, out / f'{width}-{mode[0]}-p{pass_number + 1}', width, mode, neutral)
                    run['pass'] = pass_number + 1
                    report['runs'].append(run)
                    write_reports(out, report)
                    print(f'{run["fps"]:.2f} FPS; p95 {run["gap_ms_p95"]:.2f} ms; '
                          f'fallback {run["presentation"]["fallback"]}', flush=True)
    finally:
        report['inputs_unchanged'] = all(digest(getattr(args, name)) == value
                                         for name, value in fingerprints.items())
        write_reports(out, report)
        assert report['inputs_unchanged'], 'A read-only source input changed during the test'
    print(f'Reports: {out / "report.json"} and {out / "report.md"}', flush=True)


if __name__ == '__main__':
    main()
