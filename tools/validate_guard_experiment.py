"""Isolated, input-driven Guard checks. State assertions only; no framebuffer.

Never writes the supplied save state or player save. Each process owns its TCP
port and battery path. --probe keeps diagnostic records without promoting them
to passing behavior evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time

from benchmark_presentation_filters import ROOT, OWNER, DEFAULT_STATE, digest


class Session:
    def __init__(self, out, enabled, exe=None, rom=None, diagnostic_env=None):
        self.out = out
        out.mkdir()
        (out/'Settings').mkdir()
        (out/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = '+str(int(enabled))+'\n', encoding='utf-8')
        env = {k: v for k, v in os.environ.items() if not k.startswith(('GBARECOMP_', 'SWORDCRAFT3_', 'SDL_', 'LNG_'))}
        env['PATH'] = 'C:/msys64/mingw64/bin;' + env.get('PATH', '')
        for flag in ('CUSTOM_RENDERER', 'FULL_FIELD_RENDERER', 'FULL_COMBAT_RENDERER',
                     'CUSTOM_BATTLES', 'CUSTOM_GENERAL_FIELDS', 'CUSTOM_OBJECTS',
                     'CUSTOM_ROCKY', 'CUSTOM_ADDITIONAL_AREAS'):
            env['SWORDCRAFT3_' + flag] = '1'
        # Both mechanisms let shared diagnostics compare historical experiment
        # engines with the production preference-aware engines.
        env.update(SWORDCRAFT3_CUSTOM_HOST_WIDTH='384', SWORDCRAFT3_SELECT_GUARD=str(int(enabled)),
                   SWORDCRAFT3_GUARD_TRACE='1', GBARECOMP_SELFHEAL_RECOMPILE='0',
                   SWORDCRAFT3_BETA_LAUNCHER='1', SWORDCRAFT3_PORTABLE_ROOT=str(out),
                   SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        if diagnostic_env:
            env.update(diagnostic_env)
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            port = s.getsockname()[1]
        self.exe = exe or ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
        self.rom = rom or OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'
        args = [str(self.exe), '--no-launcher', '--tcp', str(port),
                '--rom', str(self.rom), '--bios', str(OWNER/'gbarecomp/bios/gba_bios.bin'),
                '--save', str(out/'isolated.eep'), '--view-width', '240', str(ROOT/'native-test.toml')]
        self.stdout = (out/'stdout.log').open('wb')
        self.stderr = (out/'stderr.log').open('wb')
        # Coverage files follow cwd, but generic asset-picker sidecars follow
        # the executable. Explicit portable data_root above disables that
        # generic sidecar persistence; cwd alone does not isolate it.
        self.process = subprocess.Popen(args, cwd=out, env=env,
            stdout=self.stdout, stderr=self.stderr, creationflags=subprocess.CREATE_NO_WINDOW)
        self.connection = None
        try:
            deadline = time.monotonic() + 30
            while self.connection is None:
                if self.process.poll() is not None:
                    raise RuntimeError('Test process exited: ' + str(out))
                try:
                    self.connection = socket.create_connection(('127.0.0.1', port), timeout=1)
                except OSError:
                    if time.monotonic() > deadline: raise
                    time.sleep(.05)
            self.connection.settimeout(30)
            self.stream = self.connection.makefile('rwb')
        except BaseException:
            self.close()
            raise

    def call(self, cmd, **kwargs):
        self.stream.write((json.dumps(dict(cmd=cmd, **kwargs))+'\n').encode())
        self.stream.flush()
        result = json.loads(self.stream.readline())
        assert result.get('ok'), result
        return result

    def close(self):
        if self.connection:
            try: self.call('quit')
            except (OSError, ValueError, AssertionError): pass
            self.stream.close()
            self.connection.close()
        try: self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=10)
        self.stdout.close()
        self.stderr.close()

    def step(self, keys=0x3ff):
        self.call('set_keyinput', value=keys)
        frame = self.call('step')['frame']
        ram = bytes.fromhex(self.call('read_iwram', addr=0, len=0x8000)['data'])
        u16 = lambda n: int.from_bytes(ram[n:n+2], 'little')
        u32 = lambda n: int.from_bytes(ram[n:n+4], 'little')
        p = 0x8c0
        return dict(frame=frame, keys=keys, root=u32(0x6ac0), phase=u32(0x6ab4),
            mode=ram[0xc], pause=ram[0xf], auto=ram[0x12], slot=ram[p+0x166],
            arena=ram[0x1a94], effect_kind=ram[0x44e], effect_stage=ram[0x44f],
            flags=u32(p+8), guard=bool(u32(p+8)&0x100), timer=u16(p+0x98),
            animation=ram[p+0x38c], player=ram[p:p+0x390].hex(),
            held=u16(0x594c), new=u16(0x5920), hashes=self.call('state_hash'))

    def case(self, state, sequence):
        self.call('savestate_load', path=str(state))
        rows = []
        for keys, count in sequence:
            rows.extend(self.step(keys) for _ in range(count))
        return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, default=DEFAULT_STATE)
    parser.add_argument('--exe', type=Path, default=ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')
    parser.add_argument('--japanese-exe', type=Path, default=ROOT/'build-native/Swordcraft3Japanese.exe')
    parser.add_argument('--probe', action='store_true')
    args = parser.parse_args()
    out = ROOT / ('validation/guard-' + str(time.time_ns()))
    out.mkdir()
    protected = {str(p): digest(p) for p in (args.state, OWNER/'gbarecomp/bios/gba_bios.bin',
        OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba')}
    binaries = {str(p): digest(p) for p in (args.exe,args.japanese_exe)}
    report = dict(passed=False, inputs=protected, executable_sha256=binaries, cases={})
    additional = {}
    field = OWNER/'validation/visible-debugger/20260827-130539-beta/captures/frame-0000019663-1787850529742/state.gbas'
    if not args.probe:
        protected[str(field)] = digest(field)
    print('Evidence: '+str(out), flush=True)
    try:
        for enabled in (False, True):
            # Production regression deliberately conflicts with the retired
            # environment flag; the saved choice must win for On AND Off.
            session = Session(out/('enabled' if enabled else 'native'), enabled,args.exe,
                diagnostic_env={'SWORDCRAFT3_SELECT_GUARD':str(int(not enabled))})
            try:
                button = 0x3fb if enabled else 0x3fd
                rows = session.case(args.state, [(0x3ff, 8), (button, 30), (0x3ff, 12)])
                label = 'select' if enabled else 'B'
                report['cases'][label] = rows
                print(label, [(r['phase'],r['mode'],r['pause'],r['auto'],r['slot'],hex(r['flags']),r['guard'],r['timer'])
                    for r in rows[::5]], flush=True)
                if not args.probe:
                    assert any(r['guard'] for r in rows[8:38]), label+' never guarded'
                    assert not any(r['guard'] for r in rows[-5:]), label+' did not release'
                    assert all(r['auto']==0 for r in rows), label+' toggled auto-battle'
                    # Same buttons with no Select: every guest-state hash must
                    # agree. Covers ordinary B actions, R cycling and jumping.
                    ordinary = [(0x3ff,8), (0x2ff,3), (0x3ff,18), (0x3fd,8),
                                (0x3ff,10), (0x3bf,12), (0x3ff,20)]
                    additional[label] = session.case(args.state, ordinary)
                    report['cases'][label+'-ordinary'] = additional[label]
                    report['cases'][label+'-field'] = session.case(field,
                        [(0x3ff,8),(0x3fb,12),(0x3ff,12),(0x3fe,3),(0x3ff,10)])
                    assert all(r['phase']!=4 for r in report['cases'][label+'-field']), 'Field fixture not negative'
                    for name, sequence in (
                        ('airborne', [(0x3ff,8),(0x3bf,6),(button & ~0x40,16),(0x3ff,30)]),
                        ('pause-release', [(0x3ff,8),(button,12),(0x3f7,3),(0x3ff,30),
                            (0x3f7,3),(0x3ff,30),(button,15),(0x3ff,12)]),
                    ):
                        case = session.case(args.state, sequence)
                        report['cases'][label+'-'+name] = case
                        assert all(r['auto']==0 for r in case), name+' toggled AI'
                        assert not any(r['guard'] for r in case[-5:]), name+' stuck'
                    if not enabled:
                        auto_rows = session.case(args.state, [(0x3ff,8),(0x3fb,3),(0x3ff,8)])
                        assert auto_rows[-1]['auto'] in (1,2), 'Auto-on fixture failed'
                        session.call('savestate_save', path=str(out/'auto-on.gbas'))
                    else:
                        for slot in range(6):
                            prefix = [(0x3ff,8)] + [(k,n) for _ in range(slot) for k,n in ((0x2ff,3),(0x3ff,15))]
                            case = session.case(args.state,prefix+[(button,24),(0x3ff,12)])
                            begin = sum(n for _,n in prefix)
                            observed = case[begin-1]['slot']
                            assert observed==slot, ('R did not select expected slot',slot,observed)
                            assert all(r['slot']==slot for r in case[begin:]), ('Selected slot changed',slot)
                            assert all(r['auto']==0 for r in case), ('AI toggle',slot)
                            assert any(r['guard'] for r in case[begin:begin+24]), ('No guard',slot)
                            assert not any(r['guard'] for r in case[-5:]), ('Stuck guard',slot)
                            report['cases']['slot-'+str(slot)] = case
                        case = session.case(out/'auto-on.gbas', [(0x3ff,3),(button,30),(0x3ff,12)])
                        assert all(r['auto']==0 for r in case[8:]), 'Existing auto mode did not cancel'
                        assert any(r['guard'] for r in case[8:33]), 'No guard after cancelling AI'
                        report['cases']['auto-cancel'] = case
                        session.case(args.state,[(0x3ff,8),(button,12)])
                        session.call('savestate_save',path=str(out/'guard-held.gbas'))
                        case = session.case(out/'guard-held.gbas',[(0x3ff,12)])
                        assert not any(r['guard'] for r in case[-5:]), 'Restored hold stuck'
                        report['cases']['restore-release'] = case
            finally: session.close()
        if not args.probe:
            native, modified = report['cases']['B'], report['cases']['select']
            assert [r['guard'] for r in native] == [r['guard'] for r in modified]
            assert [r['timer'] for r in native] == [r['timer'] for r in modified]
            assert [r['player'] for r in native] == [r['player'] for r in modified], 'Native player state differs'
            assert [r['hashes'] for r in additional['B']] == [r['hashes'] for r in additional['select']], 'Non-Select regression'
            assert [r['hashes'] for r in report['cases']['B-field']] == [r['hashes'] for r in report['cases']['select-field']], 'Field Select behavior changed'
            for name in ('airborne','pause-release'):
                assert [r['player'] for r in report['cases']['B-'+name]] == [r['player'] for r in report['cases']['select-'+name]], name+' native player state differs'
            # Independently boot the Japanese corpus, never load an English
            # snapshot into it. Combat coverage here remains English-only.
            jp_rom = OWNER/'release/swordcraft3_jp - Copy.gba'
            protected[str(jp_rom)] = digest(jp_rom)
            jp_hashes = []
            for enabled in (False,True):
                jp = Session(out/('jp-on' if enabled else 'jp-off'),enabled,
                    args.japanese_exe,jp_rom,
                    diagnostic_env={'SWORDCRAFT3_SELECT_GUARD':str(int(not enabled))})
                try:
                    rows = []
                    for key,count in ((0x3ff,120),(0x3fe,3),(0x3ff,180),(0x3fb,3),(0x3ff,60)):
                        for _ in range(count): rows.append(jp.step(key))
                    assert all(r['phase']!=4 for r in rows)
                    jp_hashes.append([r['hashes'] for r in rows])
                    report['cases']['japanese-'+str(enabled)] = rows
                finally: jp.close()
            assert jp_hashes[0] == jp_hashes[1], 'Japanese noncombat regression'
            report['passed'] = True
            print('PASS: native player-state parity, all six slots, ordinary-input equality, airborne/pause gates, AI cancellation and restore/release', flush=True)
    finally:
        report['inputs_unchanged'] = all(digest(Path(p)) == h for p,h in protected.items())
        report['executables_unchanged'] = all(digest(Path(p)) == h for p,h in binaries.items())
        (out/'report.json').write_text(json.dumps(report, indent=2))
        assert report['inputs_unchanged']
        assert report['executables_unchanged']


if __name__ == '__main__':
    main()
