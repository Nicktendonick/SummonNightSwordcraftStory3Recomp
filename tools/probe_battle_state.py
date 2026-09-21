"""Read-only guest-state investigation over TCP; never requests framebuffer data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OWNER = ROOT.parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--state', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--sequence', default='1023:6,1015:1,1023:20,1015:1,1023:20')
    p.add_argument('--exe', type=Path, default=ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')
    p.add_argument('--compact', action='store_true', help='Keep hashes/traces, not full per-frame RAM dumps')
    p.add_argument('--draw-audit', action='store_true', help='Retain IWRAM and OAM for source-bounded submission checks')
    a = p.parse_args()
    a.exe = a.exe.resolve()
    out = a.output.resolve()
    assert out.is_relative_to(ROOT/'validation')
    out.mkdir(parents=True, exist_ok=False)
    state = a.state.resolve()
    digest = hashlib.sha256(state.read_bytes()).hexdigest()
    keys = [int(k, 0) for segment in a.sequence.split(',') for k, n in [segment.split(':')] for _ in range(int(n))]
    env = os.environ.copy()
    for k in ('GBARECOMP_INPUT_REPLAY', 'GBARECOMP_INPUT_RECORD', 'GBARECOMP_VISIBLE_DEBUGGER', 'GBARECOMP_DEBUG_CAPTURE_DIR'):
        env.pop(k, None)
    env.update(SWORDCRAFT3_CUSTOM_RENDERER='1', SWORDCRAFT3_CUSTOM_HOST_WIDTH='384',
               SWORDCRAFT3_CUSTOM_BATTLES='1', SWORDCRAFT3_CUSTOM_OBJECTS='1',
               SWORDCRAFT3_CUSTOM_REPLAY_CHECK='0', SWORDCRAFT3_CUSTOM_AUDIT='1',
               SWORDCRAFT3_STATE_TRACE='1', GBARECOMP_SELFHEAL_RECOMPILE='0',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
    args = [str(a.exe), '--tcp', str(port), '--bios', str(OWNER/'gbarecomp/bios/gba_bios.bin'),
            '--rom', str(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
            '--save', str(out/'isolated.eep'), '--view-width', '240', str(ROOT/'native-test.toml')]
    records = []
    with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
        process = subprocess.Popen(args, cwd=ROOT/'build-native', env=env, stdout=stdout, stderr=stderr)
        connection = None
        try:
            deadline = time.monotonic()+30
            while connection is None:
                if process.poll() is not None:
                    raise RuntimeError('Game exited before debugger connection')
                try:
                    connection = socket.create_connection(('127.0.0.1', port), timeout=1)
                except OSError:
                    if time.monotonic()>deadline:
                        raise
                    time.sleep(.1)
            connection.settimeout(40)
            with connection.makefile('rwb') as stream:
                def call(cmd, **kw):
                    stream.write((json.dumps(dict(cmd=cmd, **kw))+'\n').encode())
                    stream.flush()
                    r = json.loads(stream.readline())
                    assert r.get('ok'), r
                    return r
                call('savestate_load', path=str(state))
                for i, key in enumerate(keys):
                    call('set_keyinput', value=key)
                    frame = call('step')['frame']
                    row = dict(index=i, frame=frame, key=key, hashes=call('state_hash'))
                    regions=[('iwram', 0x8000), ('io', 0x400), ('ewram', 0x40000)]
                    if a.draw_audit:
                        regions.append(('oam',0x400))
                    for region, size in regions:
                        data = bytes.fromhex(call('read_'+region, addr=0, len=size)['data'])
                        if not a.compact or (a.draw_audit and region in ('iwram','oam')):
                            (out/f'{i:04d}-{region}.bin').write_bytes(data)
                        if region == 'iwram':
                            u16 = lambda offset: int.from_bytes(data[offset:offset+2], 'little')
                            u32 = lambda offset: int.from_bytes(data[offset:offset+4], 'little')
                            row['battle'] = dict(root=u32(0x6ac0), phase=u32(0x6ab4),
                                mode=data[0xc], pause=data[0xf], arena=data[0x1a94],
                                planned=u16(0x1d30), camera_y=u16(0x1a9a),
                                player_height=u32(0x8c0+0x18c), slot=data[0x8c0+0x166])
                    records.append(row)
                    if i in (1, len(keys)-1):
                        (out/f'{i:04d}-trace.json').write_text(json.dumps(call('runtime_trace', count=4096)))
                        (out/f'{i:04d}-mmio.json').write_text(json.dumps(call('mmio_cap', count=4096)))
                call('quit')
            process.wait(timeout=15)
            assert process.returncode == 0
        finally:
            if connection:
                connection.close()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            (out/'frames.json').write_text(json.dumps(records))
    assert hashlib.sha256(state.read_bytes()).hexdigest() == digest
    (out/'identity.json').write_text(json.dumps(dict(state=str(state), state_sha256=digest,
        executable=str(a.exe), executable_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(),
        sequence=a.sequence, host_width=384, frames=len(records))))
    print(f'State-only capture: {len(records)} frames at width 384; source unchanged; {out}', flush=True)


if __name__ == '__main__':
    main()
