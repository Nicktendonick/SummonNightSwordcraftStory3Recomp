"""Windowed, real-audio present cadence benchmark. No renderer/pixel assertions.

Stages both binaries with identical settings, isolates saves, closes only its own
child game window, and measures SDL presents after three seconds of warm-up.
Unlike the TCP harness this uses the normal window/audio/pacing path.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time

from validate_full_field import cases, ROOT, OWNER


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def analyze(path, warmup=3):
    rows = list(csv.DictReader(path.open(newline='')))
    gaps = [int(r['gap_us']) / 1000 for r in rows
            if float(r['t_ms']) >= warmup * 1000 and int(r['gap_us']) > 0]
    assert len(gaps) >= 100, 'Insufficient windowed samples'
    ordered = sorted(gaps)
    pct = lambda p: ordered[int((len(ordered)-1)*p)]
    return dict(presents=len(rows), measured_intervals=len(gaps),
                measured_seconds=sum(gaps)/1000, fps=1000/statistics.mean(gaps),
                gap_ms_p50=pct(.5), gap_ms_p95=pct(.95), gap_ms_p99=pct(.99),
                longest_gap_ms=max(gaps), gaps_over_25ms=sum(g>25 for g in gaps),
                gaps_over_33_5ms=sum(g>33.5 for g in gaps),
                fullscreen_samples=sum(int(r['fullscreen']) for r in rows))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--seconds', type=int, default=25)
    p.add_argument('--cases', nargs='+', default=['field-49','field-364','battle-r-jump'])
    p.add_argument('--passes', type=int, choices=[1,2], default=2)
    p.add_argument('--normal-cache', action='store_true',
                   help='Enable normal self-healing using isolated copies of the playtest cache')
    p.add_argument('--before-exe', type=Path,
                   help='Override the preserved comparison executable')
    p.add_argument('--before-full-field', action='store_true',
                   help='Use complete-frame fields in the baseline too (engine-only comparison)')
    a = p.parse_args()
    assert 8 <= a.seconds <= 60
    out = a.output.resolve()
    assert out.is_relative_to(ROOT/'validation') and not out.exists()
    sources = dict((n,s) for n,s,_,_ in cases())
    assert set(a.cases) <= sources.keys()
    out.mkdir(parents=True)
    binaries = {
        'before': a.before_exe.resolve() if a.before_exe else ROOT/'validation/full-field-20260924/accepted/Swordcraft3CustomRendererBeta.exe',
        'after': ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'}
    staged = {}
    for name, exe in binaries.items():
        folder = out/name
        folder.mkdir()
        for f in [exe, *exe.parent.glob('*.dll')]:
            shutil.copy2(f, folder/f.name)
        # Same current user settings for both, copied; never modify originals.
        for file in ('config.ini','keybinds.ini'):
            src = ROOT/'build-native'/file
            if src.exists(): shutil.copy2(src,folder/file)
        staged[name] = folder/exe.name
    private_rom = out/'private.gba'
    shutil.copy2(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba', private_rom)
    # Neutral guest input, while preserving ordinary Assist/rewind overhead.
    (out/'neutral.trace').write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n')
    report = dict(method='SDL present entry intervals, windowed, real audio, 384x160 at 3x',
                  warmup_seconds=3, run_seconds=a.seconds, input='neutral; no exclusive replay',
                  self_heal_enabled=a.normal_cache,
                  before_full_field=a.before_full_field,
                  exe_sha256={n:digest(e) for n,e in binaries.items()},
                  config_sha256={f:digest(ROOT/'build-native'/f) for f in ('config.ini','keybinds.ini')
                                 if (ROOT/'build-native'/f).exists()}, runs=[])
    for name in a.cases:
        state = sources[name]
        state_hash = digest(state)
        order = ['before','after'] if a.passes==1 else ['before','after','after','before']
        for index, mode in enumerate(order):
            run = out/f'{name}-{index}-{mode}'
            run.mkdir()
            if a.normal_cache:
                cache = ROOT/'build-native/full-combat-playtest/recomp_cache'
                if cache.exists(): shutil.copytree(cache,run/'recomp_cache')
            env = {k:v for k,v in os.environ.items()
                   if not k.startswith(('SWORDCRAFT3_', 'GBARECOMP_', 'SDL_'))}
            env.update(PATH='C:\\msys64\\mingw64\\bin;'+env.get('PATH',''),
                       SWORDCRAFT3_CUSTOM_RENDERER='1', SWORDCRAFT3_CUSTOM_HOST_WIDTH='384',
                       SWORDCRAFT3_FULL_FIELD_RENDERER=str(int(mode=='after' or a.before_full_field)),
                       SWORDCRAFT3_FULL_COMBAT_RENDERER='1', SWORDCRAFT3_CUSTOM_GENERAL_FIELDS='1',
                       SWORDCRAFT3_CUSTOM_BATTLES='1', SWORDCRAFT3_CUSTOM_OBJECTS='1',
                       SWORDCRAFT3_CUSTOM_ROCKY='1', SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS='1',
                       SWORDCRAFT3_CUSTOM_REPLAY_CHECK='0', GBARECOMP_PHASE_PROF='0',
                       GBARECOMP_SELFHEAL_RECOMPILE=str(int(a.normal_cache)), GBARECOMP_PRESENT_CADENCE='1',
                       GBARECOMP_PRESENT_CADENCE_DUMP=str(run/'cadence.csv'),
                       GBARECOMP_FRAME_PHASE=str(run/'frame-phase.csv'),
                       GBARECOMP_AUDIO_PROBE='1', GBARECOMP_INPUT_REPLAY=str(out/'neutral.trace'))
            cmd = [str(staged[mode]), '--window','--no-launcher','--scale','3',
                   '--bios',str(OWNER/'gbarecomp/bios/gba_bios.bin'), '--rom',str(private_rom),
                   '--save',str(run/'private.eep'),'--load-state',str(state),
                   '--view-width','240',str(ROOT/'native-test.toml')]
            print(f'Starting {name} {index} {mode}',flush=True)
            with (run/'stdout.log').open('wb') as stdout, (run/'stderr.log').open('wb') as stderr:
                child = subprocess.Popen(cmd,cwd=run,env=env,stdout=stdout,stderr=stderr,
                                         creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    # Start the timed session at the confirmed successful state load.
                    deadline = time.monotonic()+30
                    while 'savestate_loaded' not in (run/'stdout.log').read_text(errors='replace'):
                        assert child.poll() is None, 'Game exited before state load'
                        assert time.monotonic()<deadline, 'State load timed out'
                        time.sleep(.1)
                    try:
                        child.wait(timeout=a.seconds)
                        raise RuntimeError('Game exited before benchmark finished')
                    except subprocess.TimeoutExpired:
                        pass
                    # Graceful WM_CLOSE via .NET, only for the child we created.
                    subprocess.run(['powershell','-NoProfile','-Command',
                        f'$p=Get-Process -Id {child.pid}; if (!$p.CloseMainWindow()) {{ exit 2 }}'],
                        check=True,creationflags=subprocess.CREATE_NO_WINDOW,
                        stdout=subprocess.DEVNULL,timeout=10)
                    child.wait(timeout=20)
                    assert child.returncode==0, f'Game exit {child.returncode}'
                finally:
                    if child.poll() is None:
                        child.terminate()
                        child.wait(timeout=10)
            log = (run/'stderr.log').read_text(errors='replace')
            assert '[gba-audio-probe]' in log, 'No real audio processing evidence'
            assert '[present-cadence]' in log
            result = dict(case=name,mode=mode,order=index,state=str(state),state_sha256=state_hash,
                          **analyze(run/'cadence.csv'))
            result['diagnostics'] = [s for s in log.splitlines() if any(t in s for t in
                ('host_window:', '[gba-audio-probe]', '[sc3:custom]', '[sc3:host]', '[sc3:battle]', '[present-cadence]'))]
            report['runs'].append(result)
            assert digest(state)==state_hash, 'Input state modified'
            (out/'report.json').write_text(json.dumps(report,indent=2))
            print(f'{name} {mode}: {result["fps"]:.2f} FPS, p95 {result["gap_ms_p95"]:.2f} ms',flush=True)


if __name__=='__main__': main()
