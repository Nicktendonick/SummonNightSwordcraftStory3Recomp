"""Read-only gameplay diagnosis: compare warmed frame batches on the local TCP server.

Uses private battery files and restores the same input snapshot before every batch.
No rendering changes. Timings exclude startup, save loading and screenshot transfer;
they measure headless processing throughput, not displayed FPS or audio pacing.
"""
import argparse, hashlib, json, os, socket, statistics, subprocess, time
from pathlib import Path
from validate_field_objects_tcp import ROOT, OWNER, png


def run(state, out, custom, battles, width, count, repeats, replay_check=1, phase_profile=1):
    out.mkdir()
    env = os.environ.copy()
    for key in ('GBARECOMP_INPUT_REPLAY', 'GBARECOMP_INPUT_RECORD',
                'GBARECOMP_VISIBLE_DEBUGGER', 'GBARECOMP_DEBUG_CAPTURE_DIR',
                'GBARECOMP_SAMPLE', 'SWORDCRAFT3_CUSTOM_AUDIT'):
        env.pop(key, None)
    env.update(SWORDCRAFT3_CUSTOM_RENDERER=str(custom),
               SWORDCRAFT3_CUSTOM_BATTLES=str(battles),
               SWORDCRAFT3_CUSTOM_OBJECTS='1', SWORDCRAFT3_CUSTOM_HOST_WIDTH=str(width),
               SWORDCRAFT3_CUSTOM_REPLAY_CHECK=str(replay_check),
               GBARECOMP_PHASE_PROF=str(phase_profile), GBARECOMP_SELFHEAL_RECOMPILE='0',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    args = [str(ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'), '--tcp', str(port),
            '--bios', str(OWNER/'gbarecomp/bios/gba_bios.bin'),
            '--rom', str(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
            '--save', str(out/'private.eep'), '--view-width', '240', str(ROOT/'native-test.toml')]
    samples, states = [], []
    with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
        process = subprocess.Popen(args, cwd=out, env=env, stdout=stdout, stderr=stderr)
        connection = None
        try:
            deadline = time.monotonic()+30
            while connection is None:
                if process.poll() is not None:
                    raise RuntimeError('Game exited before TCP connection')
                try:
                    connection = socket.create_connection(('127.0.0.1', port), timeout=1)
                except OSError:
                    if time.monotonic()>deadline:
                        raise
                    time.sleep(.1)
            connection.settimeout(60)
            stream = connection.makefile('rwb')
            def call(cmd, **kwargs):
                stream.write((json.dumps(dict(cmd=cmd, **kwargs))+'\n').encode())
                stream.flush()
                result = json.loads(stream.readline())
                if not result.get('ok'):
                    raise RuntimeError(str(result))
                return result
            call('savestate_load', path=str(state))
            call('run_frames', n=60, keyinput=1023)
            for index in range(repeats):
                call('savestate_load', path=str(state))
                # Complete capture and warm the restored scene, outside timing.
                call('run_frames', n=30, keyinput=1023)
                start = time.perf_counter()
                result = call('run_frames', n=count, keyinput=1023)
                seconds = time.perf_counter()-start
                samples.append(dict(seconds=seconds, ms_per_frame=seconds*1000/count,
                                    processing_fps=count/seconds, response=result))
                states.append(call('state_hash'))
            shot = call('host_screenshot' if custom else 'screenshot')
            png(out/'final.png', shot['w'], shot['h'], bytes.fromhex(shot['data']))
            call('quit')
            stream.close()
            connection.close()
            connection = None
            process.wait(timeout=15)
            assert process.returncode == 0
        finally:
            if connection:
                connection.close()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    return dict(custom=custom, battles=battles, width=width, replay_check=replay_check,
                phase_profile=phase_profile, frames_per_batch=count,
                median_ms_per_frame=statistics.median(s['ms_per_frame'] for s in samples),
                samples=samples, end_states=states)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=240)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--compare-replay', action='store_true')
    parser.add_argument('--no-phase-profile', action='store_true', help='Disable internal scanline timing instrumentation')
    parser.add_argument('--reverse-order', action='store_true', help='Reverse mode order to check warm-up/order bias')
    args = parser.parse_args()
    state, out = args.state.resolve(), args.output_dir.resolve()
    assert out.is_relative_to(ROOT/'validation') and 1<=args.frames<=600 and 1<=args.repeats<=5
    out.mkdir(exist_ok=False)
    source_hash = hashlib.sha256(state.read_bytes()).hexdigest()
    report = dict(source_sha256=source_hash, executable_sha256=hashlib.sha256(
        (ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest(), runs={})
    modes = (('native',0,0,240,1), ('oracle-only',1,0,384,1),
             ('combat-384',1,1,384,1), ('combat-284',1,1,284,1))
    if args.compare_replay:
        modes = (('checked-384',1,1,384,1), ('reuse-384',1,1,384,0))
    if args.reverse_order:
        modes = tuple(reversed(modes))
    for name, custom, battles, width, replay_check in modes:
        result = run(state, out/name, custom, battles, width, args.frames, args.repeats,
                     replay_check, int(not args.no_phase_profile))
        report['runs'][name] = result
        (out/'report.json').write_text(json.dumps(report, indent=2))
        print(name, f"{result['median_ms_per_frame']:.3f} ms/frame", flush=True)
    report['source_unchanged'] = source_hash == hashlib.sha256(state.read_bytes()).hexdigest()
    (out/'report.json').write_text(json.dumps(report, indent=2))
    assert report['source_unchanged']


if __name__ == '__main__':
    main()
