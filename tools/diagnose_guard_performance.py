"""Read-only comparison of shipped Guard binaries and private input states.

No game code edits, screenshots, framebuffer comparisons or player-data writes.
Every child has private saves/settings; source identities are checked at exit.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import statistics
import time
from types import SimpleNamespace

import benchmark_presentation_filters as bench
from validate_guard_experiment import Session

ROOT, OWNER = bench.ROOT, bench.OWNER
PACKAGE = OWNER / 'release/Guard Experiment 20260929-023325-a2fa'
OLD = OWNER / 'release/Portable Beta/Runtime/Swordcraft3CustomRendererBeta.exe'
NEW = PACKAGE / 'Runtime/Swordcraft3CustomRendererBeta.exe'


def field_probe(out, state, executable, enabled, select, button=0x3fb, resume=False):
    session = Session(out, enabled, executable, diagnostic_env={
        'SWORDCRAFT3_STATE_TRACE': '1', 'SWORDCRAFT3_CUSTOM_AUDIT': '1',
        'SWORDCRAFT3_CUSTOM_AUDIT_DETAIL': '1'})
    rows = []
    try:
        session.call('savestate_load', path=str(state))
        if resume:
            # Native Start input only; no RAM edits to bypass the pause state.
            session.call('run_frames', n=3, keyinput=0x3f7)
            session.call('run_frames', n=30, keyinput=0x3ff)
        sequence = [(0x3ff, 30), (button, 1), (0x3ff, 24),
                    (button, 12), (0x3ff, 30)]
        for keys, count in sequence:
            for _ in range(count):
                row = session.step(keys if select else 0x3ff)
                ram = bytes.fromhex(session.call('read_iwram', addr=0, len=0x8000)['data'])
                pointer = int.from_bytes(ram[0x6b54:0x6b58], 'little')
                row['field_pointer'] = pointer
                if 0x02000000 <= pointer <= 0x0203ff00:
                    field = bytes.fromhex(session.call('read_ewram', addr=pointer-0x02000000, len=0x100)['data'])
                    row['field_record'] = field.hex()
                    row['field_flags'] = int.from_bytes(field[:2], 'little')
                row['io'] = session.call('read_io', addr=0, len=0x60)['data']
                rows.append(row)
    finally:
        session.close()
    log = (out/'stderr.log').read_text(errors='replace')
    fields = re.findall(r'\[sc3:field-frame\] ([^\r\n]+)', log)
    battles = re.findall(r'\[sc3:state-frame\] ([^\r\n]+)', log)
    compositions = re.findall(r'\[sc3:composition\] ([^\r\n]+)', log)
    guards = [line for line in log.splitlines() if '[sc3:guard]' in line]
    result = dict(state=str(state), executable=str(executable), guard=enabled,
                  select=select, button=button, rows=rows, field_frames=fields,
                  battle_frames=battles, compositions=compositions, guard_logs=guards)
    (out/'state-report.json').write_text(json.dumps(result, indent=2))
    reasons = Counter(re.search(r'reason=(\S+)', line)[1] for line in fields)
    print(out.name, 'reasons=', dict(reasons), 'guard_logs=', len(guards), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('field', 'battle', 'window', 'throughput'))
    parser.add_argument('--state', type=Path)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    args.state = args.state or (bench.DEFAULT_STATE if args.kind == 'battle' else PACKAGE/'Save States/beta.state5')
    out = ROOT / ('validation/guard-diagnosis-' + args.kind + '-' + str(time.time_ns()))
    out.mkdir()
    protected = {str(path): bench.digest(path) for path in (OLD, NEW, args.state,
        bench.DEFAULT_STATE, OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',
        OWNER/'gbarecomp/bios/gba_bios.bin')}
    report = dict(kind=args.kind, inputs_sha256=protected, runs=[])
    print('Evidence:', out, flush=True)
    try:
        if args.kind == 'field':
            for label, exe, enabled, select in (
                ('old-select', OLD, False, True),
                ('new-off-select', NEW, False, True),
                ('new-on-select', NEW, True, True),
                ('new-on-neutral', NEW, True, False)):
                result = field_probe(out/label, args.state, exe, enabled, select, resume=args.resume)
                report['runs'].append(result)
            hashes = [[row['hashes'] for row in run['rows']] for run in report['runs'][:3]]
            report['old_off_on_guest_equal'] = hashes[0] == hashes[1] == hashes[2]
            frames = [run['field_frames'] for run in report['runs'][:3]]
            report['old_off_on_renderer_equal'] = frames[0] == frames[1] == frames[2]
            print('Guest equality:', report['old_off_on_guest_equal'],
                  'renderer equality:', report['old_off_on_renderer_equal'], flush=True)
        elif args.kind == 'battle':
            for label, exe, enabled, button in (
                ('old-B', OLD, False, 0x3fd),
                ('new-off-B', NEW, False, 0x3fd),
                ('new-on-Select', NEW, True, 0x3fb)):
                report['runs'].append(field_probe(out/label, args.state,
                                                 exe, enabled, True, button, args.resume))
            runs = report['runs']
            report['player_equal'] = all([r['player'] for r in run['rows']] ==
                [r['player'] for r in runs[0]['rows']] for run in runs)
            report['composition_equal'] = all(run['compositions'] == runs[0]['compositions'] for run in runs)
            print('Native B versus Select: player equality', report['player_equal'],
                  'renderer ownership equality', report['composition_equal'], flush=True)
        elif args.kind == 'window':
            run_args = SimpleNamespace(frames=600, warmup_frames=90, strength=35,
                state=bench.DEFAULT_STATE, rom=OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',
                bios=OWNER/'gbarecomp/bios/gba_bios.bin')
            original_environment = bench.environment
            # All measurements use identical renderer flags/settings, real audio,
            # exclusive neutral replay, and no Guard/state traces.
            neutral = out/'neutral.trace'
            neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
            cases = [('old-nearest', OLD, False, bench.MODES[0]),
                     ('off-nearest', NEW, False, bench.MODES[0]),
                     ('on-nearest', NEW, True, bench.MODES[0]),
                     ('on-smooth', NEW, True, bench.MODES[2])]
            for repeat in range(2):
                for label, exe, enabled, mode in (cases if repeat == 0 else reversed(cases)):
                    def environment(run, width, trace, audit=False):
                        env = original_environment(run, width, trace, audit)
                        (run/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = '+str(int(enabled))+'\n', encoding='utf-8')
                        env.update(SWORDCRAFT3_SELECT_GUARD=str(int(enabled)), SWORDCRAFT3_GUARD_TRACE='0')
                        return env
                    bench.environment = environment
                    run_args.exe = exe
                    print('Window:', label, repeat+1, flush=True)
                    run = bench.execute(run_args, out/(label+'-'+str(repeat+1)), 384, mode, neutral)
                    run.update(label=label, repeat=repeat+1)
                    report['runs'].append(run)
                    print(f"{run['fps']:.2f} FPS, p95 {run['gap_ms_p95']:.2f} ms", flush=True)
                    (out/'report.json').write_text(json.dumps(report, indent=2))
        else:
            cases = [('old-neutral', OLD, False, 0x3ff),
                     ('off-neutral', NEW, False, 0x3ff),
                     ('on-neutral', NEW, True, 0x3ff),
                     ('off-native-guard', NEW, False, 0x3fd),
                     ('on-select-guard', NEW, True, 0x3fb)]
            for repeat in range(2):
                for label, exe, enabled, keys in (cases if repeat == 0 else reversed(cases)):
                    session = Session(out/(label+'-'+str(repeat+1)), enabled, exe,
                                      diagnostic_env={'SWORDCRAFT3_GUARD_TRACE':'0', 'GBARECOMP_PHASE_PROF':'0'})
                    samples = []
                    try:
                        for batch in range(3):
                            session.call('savestate_load', path=str(bench.DEFAULT_STATE))
                            session.call('run_frames', n=60, keyinput=0x3ff)
                            started = time.perf_counter()
                            session.call('run_frames', n=180, keyinput=keys)
                            ms = (time.perf_counter()-started)*1000/180
                            samples.append(dict(ms_per_frame=ms, state=session.call('state_hash')))
                    finally: session.close()
                    median = statistics.median(s['ms_per_frame'] for s in samples)
                    report['runs'].append(dict(label=label, repeat=repeat+1, samples=samples, median_ms=median))
                    print(label, repeat+1, f'{median:.3f} ms/frame (uncapped, not displayed FPS)', flush=True)
                    (out/'report.json').write_text(json.dumps(report, indent=2))
    finally:
        report['inputs_unchanged'] = all(bench.digest(Path(path)) == sha for path, sha in protected.items())
        (out/'report.json').write_text(json.dumps(report, indent=2))
        assert report['inputs_unchanged'], 'Source input changed during diagnosis'


if __name__ == '__main__':
    main()
