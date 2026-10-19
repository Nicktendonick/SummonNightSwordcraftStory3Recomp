"""Private packaged launcher/language/graphics regression, without pixel assertions.

Uses the actual launcher model and runtime menu via their scripted-input APIs.
All writable data is in a NEW explicit output folder, never the supplied package.
Real desktop graphics are required for the launcher; audio is dummy. This is not
physical-controller, human-visual, performance or clean-machine certification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TARGET = json.loads((ROOT / 'packaging/portable-beta/target.json').read_text())
ENGLISH_SAVE = 'english-' + TARGET['english_revision'] + '.eep'
ENGLISH_LABEL = 'English (translation ' + TARGET['english_revision'].removesuffix('f') + '.f)'
PRESETS = ((0, 0, 35), (2, 0, 35), (3, 0, 35), (0, 1, 25), (1, 2, 35))


def digest(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


def tree(path):
    return {str(p.relative_to(path)): digest(p) for p in path.rglob('*') if p.is_file()}


def execute(exe, arguments, output, env, name):
    with (output / (name + '.out')).open('wb') as stdout, (output / (name + '.err')).open('wb') as stderr:
        child = subprocess.Popen([str(exe), *arguments], cwd=output, env=env,
            stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=45)
        except subprocess.TimeoutExpired:
            # Only this still-live test supervisor and its own descendants.
            if child.poll() is None:
                taskkill = Path(os.environ['SystemRoot']) / 'System32/taskkill.exe'
                result = subprocess.run([str(taskkill), '/PID', str(child.pid), '/T', '/F'],
                    capture_output=True, text=True, timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
                (output / (name + '.timeout.txt')).write_text(result.stdout + result.stderr)
                child.wait(timeout=15)
            raise
    assert child.returncode == 0, (name, child.returncode)
    return (output / (name + '.err')).read_text(errors='replace'), (output / (name + '.out')).read_text(errors='replace')


def runtime_filters(log):
    return [tuple(map(int, values)) for values in re.findall(
        r'\[runtime-filters\] scaling=(\d+) effect=(\d+) strength=(\d+)', log)]


def launcher_filters(log):
    rows = re.findall(r'\[launcher:filters\] scaler=.*? linear=(\d+) sharp=(\d+) smooth=(\d+) effect=(\d+) strength=(\d+)', log)
    return [(3 if int(smooth) else 2 if int(sharp) else int(linear), int(effect), int(strength))
            for linear, sharp, smooth, effect, strength in rows]


def launcher_command(package):
    # --frames (like --rom/--load-state) deliberately bypasses launcher_preboot,
    # even with --launcher. The bounded menu script closes the child instead.
    return ['--window', '--launcher', '--view-width', '240', str(package / 'Runtime/game.toml')]


def runtime_aspects(log):
    return [tuple(map(int, values)) for values in re.findall(
        r'\[runtime-aspect\] selected=(\d+) applied=(\d+) host=(\d+) guest=(\d+)', log)]


def verify_applied_aspect(log, index, width):
    rows = runtime_aspects(log)
    assert rows and rows[-1] == (index, index, width, 240), rows


def finalize_report(report, protected, package, before):
    report['inputs_and_test_batteries_unchanged'] = all(
        Path(path).is_file() and digest(Path(path)) == value for path, value in protected.items())
    report['package_unchanged'] = tree(package) == before
    report['passed'] = (report['passed'] and report['inputs_and_test_batteries_unchanged']
                        and report['package_unchanged'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('package', 'japanese_rom', 'english_rom', 'bios', 'patch', 'output'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    args = parser.parse_args()
    for name in ('package', 'japanese_rom', 'english_rom', 'bios', 'patch'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    output = args.output.resolve()
    if output.exists() or not output.parent.is_dir():
        parser.error('--output must be a new folder with an existing parent')
    manifest = json.loads((args.package / 'Runtime/package-manifest.json').read_text())
    assert manifest['schema'] == 2 and manifest['target'] == TARGET
    for row in manifest['files']:
        assert digest(args.package / row['path']) == row['sha256'], row['path']
    for name in ('japanese', 'english'):
        assert digest(getattr(args, name + '_rom'), 'sha1') == TARGET[name + '_sha1']
    assert digest(args.bios, 'sha1') == TARGET['bios_sha1']
    protected = {str(path): digest(path) for path in (args.japanese_rom, args.english_rom, args.bios, args.patch)}
    package_before = tree(args.package)
    output.mkdir()
    data = output / 'data'
    for name in ('Settings', 'Saves', 'Save States', 'BIOS', 'ROMs', 'Mods', 'Logs', 'Captures', 'Credits'):
        (data / name).mkdir(parents=True)
    shutil.copyfile(args.bios, data / 'BIOS/test.bin')
    (data / 'Settings/bios.cfg').write_text('../BIOS/test.bin\n')
    for path in (args.package / 'Credits').iterdir():
        if path.is_file():
            shutil.copyfile(path, data / 'Credits' / path.name)
    batteries = [data / 'Saves' / name for name in ('japanese.eep', ENGLISH_SAVE)]
    for path in batteries:
        path.write_bytes(b'\xff' * 8192)
        protected[str(path)] = digest(path)
    invalid = output / 'unsupported.gba'
    invalid.write_bytes(b'Not a supported game input.\n')
    wrong_identity = output / 'unsupported-same-size.gba'
    wrong_identity.write_bytes(bytes(args.japanese_rom.stat().st_size))
    neutral = output / 'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n')
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('GBARECOMP_', 'SWORDCRAFT3_', 'SDL_', 'LNG_'))}
    system = Path(os.environ['SystemRoot'])
    env['PATH'] = str(system / 'System32') + os.pathsep + str(system)
    env.update(SWORDCRAFT3_BETA_LAUNCHER='1', SWORDCRAFT3_PORTABLE_ROOT=str(data),
               SWORDCRAFT3_CUSTOM_HOST_WIDTH='384', GBARECOMP_SELFHEAL_RECOMPILE='0',
               GBARECOMP_STRICT_STATIC='1',
               GBARECOMP_INPUT_REPLAY=str(neutral), GBARECOMP_INPUT_REPLAY_EXCLUSIVE='1',
               SDL_AUDIODRIVER='dummy')
    for flag in ('CUSTOM_RENDERER', 'FULL_FIELD_RENDERER', 'FULL_COMBAT_RENDERER',
                 'CUSTOM_BATTLES', 'CUSTOM_GENERAL_FIELDS', 'CUSTOM_OBJECTS', 'CUSTOM_ROCKY', 'CUSTOM_ADDITIONAL_AREAS'):
        env['SWORDCRAFT3_' + flag] = '1'
    engine = args.package / 'Runtime/Swordcraft3CustomRendererBeta.exe'
    command = launcher_command(args.package)
    report = dict(passed=False, revisions=manifest['revisions'], inputs=protected,
                  harness_sha256=digest(Path(__file__)), no_pixel_assertions=True,
                  dummy_audio=True, system_only_path=True, cases=[])

    def run(name, script, runtime='10:menu_open;20:menu_probe;30:menu_close;40:menu_confirm'):
        env['LNG_SCRIPT'] = 'wait:8;' + script
        env['GBARECOMP_ASSIST_SCRIPT'] = runtime
        return execute(engine, command, output, env, name)

    try:
        for name, script, language, save, sibling in (
            ('japanese', f'patchclear;romfile:{args.japanese_rom};romprobe;play', 'Japanese (original)', 'japanese.eep', 'Swordcraft3Japanese.exe'),
            ('english-patch', f'patchfile:{args.patch};romprobe;play', ENGLISH_LABEL, ENGLISH_SAVE, 'Swordcraft3CustomRendererBeta.exe'),
            ('japanese-return', 'patchtoggle;romprobe;play', 'Japanese (original)', 'japanese.eep', 'Swordcraft3Japanese.exe'),
            ('english-prepatched', f'patchclear;romfile:{args.english_rom};romprobe;play', ENGLISH_LABEL, ENGLISH_SAVE, 'Swordcraft3CustomRendererBeta.exe'),
        ):
            log, stdout = run(name, script)
            assert 'verified=1 can_play=1' in log and 'language=' + language in log
            assert 'engine=' + sibling in log and save in log
            assert 'event=menu_probe pump=20' in log and 'self_heal_coverage=FULLY_STATIC dispatch_misses=0' in stdout
            report['cases'].append(dict(name=name, verified_engine=sibling, save=save, passed=True))
            print(name, 'PASS', flush=True)
        selected = (data / 'Settings/rom.cfg').read_text().strip()
        imported = (data / 'Settings' / selected).resolve()
        assert imported.is_relative_to(data) and imported.name == args.english_rom.name
        assert digest(imported) == digest(args.english_rom)
        report['readable_rom_filename_preserved'] = True
        for name, path in (('unsupported', invalid), ('unsupported-same-size', wrong_identity)):
            log, stdout = run(name, f'patchclear;romfile:{path};romprobe;play;wait:3;quit')
            assert 'verified=0 can_play=0' in log and 'play blocked:' in log
            assert 'self_heal_coverage=' not in stdout and '[sc3:language]' not in log
            report['cases'].append(dict(name=name, launch_blocked=True, passed=True))
        navigation = ';'.join('view:' + page + ';wait:2;uiprobe' for page in
                              ('dashboard', 'graphics', 'audio', 'controller', 'assist_tools', 'mods', 'credits'))
        log, _ = run('navigation', navigation + ';quit')
        for page in ('Dashboard', 'Graphics', 'Audio', 'Controller', 'Assist Tools', 'Mods', 'Credits'):
            assert '[launcher:ui] organized=1 view=' + page + ' translation_section=Mods' in log, page
        report['organized_pages_and_translation_in_mods'] = True
        for index, expected in enumerate(PRESETS):
            log, _ = run('preset-' + str(index),
                f'romfile:{args.japanese_rom};view:graphics;preset:{index};presetprobe;filterprobe;play')
            assert launcher_filters(log) == [expected]
            assert runtime_filters(log) and all(row == expected for row in runtime_filters(log))
            assert '[runtime-preset] index=' + str(index) + ' ' in log
            report['cases'].append(dict(name='preset-' + str(index), launcher_runtime_match=True, passed=True))
            print('preset', index, 'PASS', flush=True)
        # Runtime -> fresh launcher, using a custom combination no preset owns.
        log, _ = run('runtime-custom', 'play',
            '5:menu_open;8:menu_scaling=3;10:menu_effect=1;12:menu_strength=42;'
            '14:menu_screen=2;16:menu_aspect=1;20:menu_probe;22:menu_resume;'
            '28:menu_probe;30:menu_open;34:menu_close;40:menu_confirm')
        assert runtime_filters(log)[-1] == (3, 1, 42)
        verify_applied_aspect(log, 1, 284)
        assert (1, 2, 384, 240) in runtime_aspects(log), 'Paused width was not deferred'
        assert '[runtime-screen] index=2 name=frontlit' in log
        log, _ = run('reload-custom', 'filterprobe;presetprobe;screenprobe;aspectprobe;quit')
        assert launcher_filters(log) == [(3, 1, 42)]
        assert '[launcher:preset] index=-1 name=Custom' in log
        assert '[launcher:screen] index=2 ' in log and '[launcher:aspect] index=1 ' in log
        report['runtime_to_launcher_custom_persistence'] = True
        for index, width in ((0, 240), (2, 384)):
            # Width changes intentionally wait for Resume; don't compare a
            # selected setting to the old paused frame's applied width.
            log, _ = run('aspect-' + str(index), 'play',
                f'5:menu_open;10:menu_aspect={index};12:menu_resume;20:menu_probe;'
                '24:menu_open;30:menu_close;40:menu_confirm')
            verify_applied_aspect(log, index, width)
            log, _ = run('reload-aspect-' + str(index), 'aspectprobe;quit')
            assert f'[launcher:aspect] index={index} ' in log
        report['three_aspects_persisted'] = True
        # Exercise a real SDL-key activation route rather than the model's
        # direct `play` command. This is still synthetic, not a physical-key test.
        log, stdout = run('keyboard-play', 'view:graphics;wait:3;key:Backspace;wait:4;key:Return;wait:12')
        assert 'event=menu_probe pump=20' in log
        assert 'self_heal_coverage=FULLY_STATIC dispatch_misses=0' in stdout
        report['scripted_keyboard_play_from_graphics'] = True
        report['passed'] = True
    finally:
        finalize_report(report, protected, args.package, package_before)
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    assert report['inputs_and_test_batteries_unchanged'] and report['package_unchanged']
    print('REPORT=' + str(output / 'report.json'), flush=True)


if __name__ == '__main__':
    main()
