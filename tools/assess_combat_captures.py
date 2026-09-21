"""Diagnose captured combat layouts and R cycling, without changing game code."""
import argparse, hashlib, json, os, socket, subprocess, time
from pathlib import Path
from validate_field_objects_tcp import ROOT, OWNER, png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--width', type=int, choices=(284,320,384), default=384)
    parser.add_argument('--expect-fixed', action='store_true', help='Assert continuous wide coverage through every R/L cycle frame')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    assert out.is_relative_to(ROOT/'validation')
    out.mkdir(exist_ok=False)
    fixtures = [
        ('width-320', 'playtest-20260913-215411-815/frame-0000006754-1789350879820'),
        ('width-284', 'playtest-20260913-215449-420/frame-0000007788-1789350931971'),
        ('forest-wide', 'playtest-20260913-215538-152/frame-0000005817-1789350991388'),
        ('forest-narrow', 'playtest-20260913-215538-152/frame-0000005975-1789350994153'),
        ('rocky-victory', 'playtest-20260913-215538-152/frame-0000037049-1789351389376')]
    originals = {name: hashlib.sha256((ROOT/'validation'/path/'state.gbas').read_bytes()).hexdigest()
                 for name, path in fixtures}
    env = os.environ.copy()
    for key in ('GBARECOMP_INPUT_REPLAY', 'GBARECOMP_INPUT_RECORD', 'GBARECOMP_VISIBLE_DEBUGGER',
                'GBARECOMP_DEBUG_CAPTURE_DIR', 'GBARECOMP_SAMPLE', 'GBARECOMP_PHASE_PROF'):
        env.pop(key, None)
    env.update(SWORDCRAFT3_CUSTOM_RENDERER='1', SWORDCRAFT3_CUSTOM_BATTLES='1',
               SWORDCRAFT3_CUSTOM_OBJECTS='1', SWORDCRAFT3_CUSTOM_HOST_WIDTH=str(args.width),
               SWORDCRAFT3_CUSTOM_AUDIT='1', GBARECOMP_SELFHEAL_RECOMPILE='0',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    command = [str(ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'), '--tcp', str(port),
               '--bios', str(OWNER/'gbarecomp/bios/gba_bios.bin'),
               '--rom', str(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
               '--save', str(out/'private.eep'), '--view-width', '240', str(ROOT/'native-test.toml')]
    rows, hashes, cycle_frames = [], [], []
    with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
        process = subprocess.Popen(command, cwd=out, env=env, stdout=stdout, stderr=stderr)
        connection = None
        try:
            deadline = time.monotonic()+30
            while connection is None:
                if process.poll() is not None:
                    raise RuntimeError('TCP game exited')
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
            def snapshot(name):
                shot = call('host_screenshot')
                native = call('screenshot')
                w, h = shot['w'], shot['h']
                rgb = bytes.fromhex(shot['data'])
                center = bytes.fromhex(native['data'])
                left = (w-240)//2
                assert b''.join(rgb[(y*w+left)*3:(y*w+left+240)*3] for y in range(h)) == center
                margin = b''.join(rgb[y*w*3:(y*w+left)*3]+rgb[(y*w+left+240)*3:(y+1)*w*3] for y in range(h))
                png(out/(name+'.png'), w, h, rgb)
                vram = bytes.fromhex(call('read_vram', addr=0, len=0x18000)['data'])
                io = bytes.fromhex(call('read_io', addr=0, len=0x400)['data'])
                (out/(name+'-vram.bin')).write_bytes(vram)
                (out/(name+'-io.bin')).write_bytes(io)
                u16 = lambda off: int.from_bytes(io[off:off+2], 'little')
                row = dict(name=name, frame=call('ppu_state')['frame'], width=w, colored_margins=any(margin),
                           near_sha1=hashlib.sha1(vram[0x3800:0x4800]).hexdigest(),
                           far_sha1=hashlib.sha1(vram[0x2800:0x3800]).hexdigest(),
                           dispcnt=hex(u16(0)), bgcnt=[hex(u16(8+i*2)) for i in range(4)])
                rows.append(row)
                hashes.append(call('state_hash'))
                print(json.dumps(row), flush=True)
                if name=='width-320':
                    (out/'mmio.json').write_text(json.dumps(call('mmio_cap',count=4096)))
                if args.expect_fixed:
                    # Rocky/Manig now has its own authenticated profile too.
                    assert row['colored_margins'], name
            def cycle_advance(n, key):
                if not args.expect_fixed:
                    call('run_frames', n=n, keyinput=key)
                    return
                call('set_keyinput', value=key)
                for _ in range(n):
                    frame = call('step')['frame']
                    shot = call('host_screenshot')
                    rgb = bytes.fromhex(shot['data'])
                    w,h=shot['w'],shot['h'];left=(w-240)//2
                    native=bytes.fromhex(call('screenshot')['data'])
                    assert b''.join(rgb[(y*w+left)*3:(y*w+left+240)*3] for y in range(h))==native
                    wide=any(b''.join(rgb[y*w*3:(y*w+left)*3]+rgb[(y*w+left+240)*3:(y+1)*w*3] for y in range(h)))
                    cycle_frames.append(dict(frame=frame,keyinput=key,wide=wide))
                    assert wide, f'Cycle dropped widescreen at {frame}'
            for name, path in fixtures:
                call('savestate_load', path=str(ROOT/'validation'/path/'state.gbas'))
                call('run_frames', n=6, keyinput=1023)
                snapshot(name)
            call('savestate_load', path=str(ROOT/'validation'/fixtures[2][1]/'state.gbas'))
            call('run_frames', n=6, keyinput=1023)
            snapshot('r-cycle-00')
            for index in range(1, 9):
                cycle_advance(3,0x02ff)
                cycle_advance(15,0x03ff)
                snapshot(f'r-cycle-{index:02}')
            if args.expect_fixed:
                for index in range(1,7):
                    cycle_advance(3,0x01ff)
                    cycle_advance(15,0x03ff)
                    snapshot(f'l-cycle-{index:02}')
            call('quit')
            stream.close()
            connection.close()
            connection = None
            process.wait(timeout=15)
            assert process.returncode == 0
        finally:
            if connection:
                try:
                    call('quit')
                    process.wait(timeout=5)
                except Exception:
                    pass
                connection.close()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    unchanged = all(originals[name] == hashlib.sha256((ROOT/'validation'/path/'state.gbas').read_bytes()).hexdigest()
                    for name, path in fixtures)
    report = dict(source_states_unchanged=unchanged, native_centers_match=True,
                  executable_sha256=hashlib.sha256((ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest(),
                  snapshots=rows, states=hashes, cycle_frames=cycle_frames,
                  continuous_cycle_coverage_checked=args.expect_fixed)
    (out/'report.json').write_text(json.dumps(report, indent=2))
    assert unchanged


if __name__ == '__main__':
    main()
