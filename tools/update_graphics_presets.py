"""Stage, exercise and promote graphics-preset/live-colour engines to Portable Beta.

Only two engines and README may change. Player data/settings are protected by
whole-install snapshots, rollback copies and isolated test roots. No publishing.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess

from package_alpha import file_hash, pe_info, check_dependencies
from update_guard_performance import ROOT, OWNER, ENGINES, DLLS, snapshot, stamp, replace_with_verified_copy
from promote_portable_beta import no_game_running, provenance, require
from benchmark_presentation_filters import environment, DEFAULT_STATE

TARGET = OWNER/'release/Portable Beta'
STARTER = 'Swordcraft Story 3 Beta.exe'
FILES = {'README.md', *(f'Runtime/{name}' for name in ENGINES)}
RECIPES = [(0, 0, 35), (2, 0, 35), (3, 0, 35), (0, 1, 25), (1, 2, 35)]
NAMES = ['Original Pixels', 'Clean & Crisp', 'Soft & Smooth', 'Handheld Grid', 'Retro TV']
MODELS = ['raw', 'unlit', 'frontlit', 'backlit', 'classic']


def prepare():
    no_game_running()
    require(TARGET.resolve(strict=True) == TARGET, 'Unexpected target')
    before = snapshot(TARGET)
    require('Runtime/package-manifest.json' not in before, 'Manifest-bearing target needs manifest-aware update')
    stage = ROOT/'validation'/('graphics-presets-'+stamp())
    payload, package = stage/'payload', stage/'package'
    payload.mkdir(parents=True); package.mkdir()
    sources, binaries = {}, {}
    for relative in sorted(FILES):
        source = ROOT/'packaging/portable-beta/README.md' if relative == 'README.md' else ROOT/'build-native'/Path(relative).name
        dest = payload/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        sources[str(source)] = file_hash(source)
        shutil.copyfile(source, dest)
        if relative.endswith('.exe'):
            subprocess.run(['C:/msys64/mingw64/bin/strip.exe', '--strip-debug', str(dest)], check=True,
                           timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            original, candidate = pe_info(source), pe_info(dest)
            for field in ('loaded_sections', 'imports', 'entry_rva'):
                require(original[field] == candidate[field], 'Stripping changed executable: '+relative)
            binaries[Path(relative).name] = candidate
    for relative in before:
        if relative == STARTER or relative.startswith(('Runtime/assets/', 'Credits/')) or relative == 'Runtime/game.toml' or relative in {'Runtime/'+n for n in DLLS}:
            dest = package/relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(TARGET/relative, dest)
            require(file_hash(dest) == before[relative], 'Dependency copy mismatch')
    for name in DLLS:
        binaries[name] = pe_info(package/'Runtime'/name)
    check_dependencies(binaries)
    for relative in FILES:
        dest = package/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(payload/relative, dest)
    require(snapshot(TARGET) == before, 'Main installation changed during staging')
    record = dict(target=str(TARGET), before=before, sources=sources, payload=snapshot(payload),
                  source_state=provenance(), dependency_closure=True, strip_preserves_code=True)
    (stage/'PREPARED.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    print(stage, flush=True)


def checked(stage):
    stage = stage.resolve(strict=True)
    require(stage.parent == ROOT/'validation' and stage.name.startswith('graphics-presets-'), 'Invalid stage')
    record = json.loads((stage/'PREPARED.json').read_text())
    require(record['target'] == str(TARGET) and TARGET.resolve(strict=True) == TARGET, 'Target identity mismatch')
    require(set(record['payload']) == FILES and snapshot(stage/'payload') == record['payload'], 'Payload changed')
    for relative, digest in record['payload'].items():
        require(file_hash(stage/'package'/relative) == digest, 'Smoke package differs from payload')
    return stage, record


def execute(exe, args, run, env, label):
    with (run/(label+'.out')).open('wb') as out, (run/(label+'.err')).open('wb') as err:
        child = subprocess.Popen([str(exe), *args], cwd=run, env=env, stdout=out, stderr=err,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=40)
        except subprocess.TimeoutExpired:
            # Only the process this test owns, never another player's game.
            child.terminate(); child.wait(timeout=10)
            raise
    require(child.returncode == 0, label+' process failed')
    return (run/(label+'.err')).read_text(errors='replace')


def smoke(stage):
    stage, record = checked(stage)
    no_game_running()
    require(snapshot(TARGET) == record['before'], 'Main install changed')
    package = stage/'package'
    protected_package = snapshot(package)
    roms = [OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba', OWNER/'release/swordcraft3_jp - Copy.gba']
    bios = OWNER/'gbarecomp/bios/gba_bios.bin'
    protected = {p:file_hash(p) for p in [*roms, bios, DEFAULT_STATE]}
    result = dict(passed=False, payload=record['payload'], languages=[])
    try:
        # Actual portable entry point, no developer DLL search path or private game data.
        system = os.environ.get('SYSTEMROOT') or os.environ.get('SystemRoot')
        require(bool(system), 'Missing system root')
        env = {k:v for k,v in os.environ.items() if not k.startswith(('SWORDCRAFT3_', 'GBARECOMP_', 'SDL_', 'LNG_'))}
        env['PATH'] = os.path.join(system, 'System32')+';'+system
        actions = ';'.join(f'preset:{i};presetprobe;filterprobe' for i in range(5))
        screens = ';screenprobe' + ';screencycle;screenprobe' * 5
        env['LNG_SCRIPT'] = 'wait:8;view:settings;'+actions+';effectstrength:45;presetprobe;preset:1'+screens+';wait:5;quit'
        before_logs = set((package/'Captures').glob('*/session.log'))
        execute(package/STARTER, [], stage, env, 'starter')
        logs = set((package/'Captures').glob('*/session.log')) - before_logs
        require(len(logs) == 1, 'Missing starter session log')
        log = next(iter(logs)).read_text(errors='replace')
        for i, name in enumerate(NAMES):
            require(f'[launcher:preset] index={i} name={name}' in log, 'Starter recipe missing')
        require('[launcher:preset] index=-1 name=Custom' in log, 'Starter Custom missing')
        for i, model in enumerate(MODELS):
            require(f'[launcher:screen] index={i} name={model.capitalize()}' in log, 'Starter screen model missing')
        for language, name, rom in zip(('english', 'japanese'), ENGINES, roms):
            run = stage/language
            (run/'Settings').mkdir(parents=True)
            ini = run/'Settings/launcher.ini'
            ini.write_text('[Launcher]\nscreen = classic\nhost_aspect_index = 2\nvolume = 73\n[Other]\nkeep = yes\n', encoding='utf-8')
            (run/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
            guard_hash = file_hash(run/'Settings/guard.ini')
            trace = run/'neutral.trace'
            trace.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
            env = environment(run, 384, trace)
            env['SDL_AUDIODRIVER'] = 'dummy'
            commands = [(1, 'menu_auto_on'), (5, 'menu_open')]
            commands += [(10 + i*5, f'menu_preset={i}') for i in range(5)]
            commands += [(35, 'menu_strength=45'), (40, 'menu_preset=99'), (45, 'menu_preset=3')]
            for i, model in enumerate([0, 1, 2, 3, 4, 0]):
                commands += [(50+i*10, f'menu_screen={model}'), (55+i*10, 'menu_probe')]
            commands += [(110, 'menu_screen=99'), (115, 'menu_screen=3'), (120, 'menu_resume'),
                         (135, 'menu_open'), (145, 'menu_reset'), (155, 'menu_confirm')]
            env['GBARECOMP_ASSIST_SCRIPT'] = ';'.join(f'{pump}:{action}' for pump, action in commands)
            env['GBARECOMP_ASSIST_SCRIPT_AFTER_RESET'] = '10:menu_open;20:menu_probe;30:menu_close;40:menu_confirm'
            exe = package/'Runtime'/name
            args = ['--window', '--no-launcher', '--frames', '180', '--bios', str(bios), '--rom', str(rom),
                    '--save', str(run/'synthetic.eep'), '--screen', 'classic', '--host-aspect', '2']
            if language == 'english':
                args += ['--load-state', str(DEFAULT_STATE)]
            args += [str(ROOT/'native-test.toml')]
            log = execute(exe, args, run, env, 'live')
            for i, (scaling, effect, strength) in enumerate(RECIPES):
                require(f'[runtime-filters] scaling={scaling} effect={effect} strength={strength}' in log, 'Wrong live recipe')
                require(f'[runtime-preset] index={i} name={NAMES[i]}' in log, 'Wrong live label')
            require(log.count('[runtime-preset] index=-1 name=Custom') >= 2, 'Custom/invalid-index handling failed')
            events = re.findall(r'event=menu_[^ ]+ pump=(\d+) frame=(\d+) cycles=(\d+) pc=(\w+)', log.split('Clean reset:')[0])
            held = [e[1:] for e in events if 5 <= int(e[0]) <= 120]
            require(len(held) == len([p for p,a in commands if 5 <= p <= 120]) and len(set(held)) == 1,
                    'Guest advanced while changing paused presets/models')
            require(any(e[1:] != held[0] for e in events if int(e[0]) == 135), 'Resume did not advance the game')
            screen_events = {}
            for match in re.finditer(r'\[runtime-menu\] event=(\S+) pump=(\d+)[^\n]*\n'
                                    r'\[runtime-filters\] ([^\n]+)\n\[runtime-preset\] [^\n]*\n'
                                    r'\[runtime-screen\] index=(\d+) name=(\w+) colour_frames=(\d+)',
                                    log.split('Clean reset:')[0]):
                screen_events[int(match[2])] = (int(match[4]), match[5], int(match[6]), match[3])
            for i, model in enumerate([0, 1, 2, 3, 4, 0]):
                change, probe = screen_events[50+i*10], screen_events[55+i*10]
                require(change[:2] == probe[:2] == (model, MODELS[model]), 'Wrong live screen selection')
                require(probe[3] == 'scaling=0 effect=1 strength=25', 'Screen model changed other filters')
                require((probe[2] > change[2]) if model else (probe[2] == change[2]),
                        'Paused presentation did not use new colour path')
            require(screen_events[110][:2] == (0, 'raw'), 'Invalid screen model changed active choice')
            reset = log.find('Clean reset:')
            require(reset >= 0 and '[runtime-preset] index=3 name=Handheld Grid' in log[reset:], 'Reset lost preset')
            require('[runtime-screen] index=3 name=backlit' in log[reset:], 'Reset lost live colour model')
            require('host_aspect_index = 2' in ini.read_text() and 'keep = yes' in ini.read_text(), 'Unrelated preference lost')
            require('screen = backlit' in ini.read_text() and 'volume = 73' in ini.read_text(), 'Colour/audio preference lost')
            require(file_hash(run/'Settings/guard.ini') == guard_hash, 'Guard preference changed')
            env.pop('GBARECOMP_ASSIST_SCRIPT'); env.pop('GBARECOMP_ASSIST_SCRIPT_AFTER_RESET')
            env['LNG_SCRIPT'] = 'wait:8;view:settings;screenprobe;presetprobe;preset:1;presetprobe;screencycle;screenprobe;wait:5;quit'
            log = execute(exe, ['--window', '--launcher', str(ROOT/'native-test.toml')], run, env, 'launcher')
            require('index=3 name=Handheld Grid' in log and 'index=1 name=Clean & Crisp' in log, 'Launcher failed to reload live preset')
            require('index=3 name=Backlit' in log and 'index=4 name=Classic' in log, 'Launcher colour reload/edit failed')
            env['LNG_SCRIPT'] = 'wait:8;view:settings;presetprobe;screenprobe;wait:5;quit'
            log = execute(exe, ['--window', '--launcher', str(ROOT/'native-test.toml')], run, env, 'reopen')
            require('index=1 name=Clean & Crisp' in log, 'Launcher preset did not survive reopening')
            require('index=4 name=Classic' in log, 'Launcher colour edit did not survive reopening')
            # Failed live persistence must not partially apply the preset.
            env.pop('LNG_SCRIPT')
            env.update(SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software',
                       GBARECOMP_ASSIST_SCRIPT='1:menu_open;5:menu_screen=0;10:menu_preset=4;15:menu_probe;20:menu_close;25:menu_confirm')
            before_ini = file_hash(ini)
            os.chmod(ini, stat.S_IREAD)
            try:
                fail_args = args[:-1]+['--sharp-filter', '1', str(ROOT/'native-test.toml')]
                log = execute(exe, fail_args, run, env, 'readonly')
                require('Could not save graphics preset; previous values retained.' in log, 'Read-only error not reported')
                require('[runtime-preset] index=1 name=Clean & Crisp' in log and 'name=Retro TV' not in log, 'Failed save applied preset')
                require('Could not apply/save screen model; previous model retained.' in log, 'Screen model save failure missing')
                require('[runtime-screen] index=4 name=classic' in log and '[runtime-screen] index=0' not in log, 'Failed save changed colour model')
                require(file_hash(ini) == before_ini, 'Read-only preferences changed')
            finally:
                os.chmod(ini, stat.S_IREAD | stat.S_IWRITE)
            result['languages'].append(dict(language=language, all_recipes=True, paused_guest_frozen=True,
                all_screen_models=True, paused_colour_path=True, reset=True, launcher_reopen=True, custom=True, readonly_safe=True))
            print('PASS: presets/colour models/live pause/Reset/launcher reopen/read-only '+language, flush=True)
        require(all(file_hash(p) == h for p,h in protected.items()), 'Private input changed')
        require(all(file_hash(package/p) == h for p,h in protected_package.items()), 'Program payload changed')
        require(snapshot(TARGET) == record['before'], 'Smoke changed main installation')
        result.update(passed=True, private_inputs_unchanged=True, main_untouched=True, starter=True)
    finally:
        (stage/'SMOKE.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(stage/'SMOKE.json', flush=True)


def install(stage):
    stage, record = checked(stage)
    no_game_running()
    require(snapshot(TARGET) == record['before'], 'Main install changed since preparation')
    tested = json.loads((stage/'SMOKE.json').read_text())
    require(tested['passed'] and tested['payload'] == record['payload'], 'Exact-payload smoke evidence absent')
    require(all(file_hash(Path(p)) == h for p,h in record['sources'].items()), 'Build sources changed')
    backup = OWNER/'release'/('Portable Beta Graphics Rollback '+stamp())
    backup.mkdir()
    for relative in FILES:
        dest = backup/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(TARGET/relative, dest)
        require(file_hash(dest) == record['before'][relative], 'Backup mismatch')
    expected = dict(record['before']); expected.update(record['payload'])
    changed = []
    try:
        no_game_running()
        require(snapshot(TARGET) == record['before'], 'Main install changed during backup')
        for relative in sorted(FILES):
            replace_with_verified_copy(stage/'payload'/relative, TARGET/relative)
            changed.append(relative)
        subprocess.run([str(TARGET/STARTER), '--check'], check=True, cwd=stage, timeout=15,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        require(snapshot(TARGET) == expected, 'Unexpected installation change')
    except BaseException:
        for relative in reversed(changed):
            replace_with_verified_copy(backup/relative, TARGET/relative)
        raise
    result = dict(installed=True, target=str(TARGET), backup=str(backup), payload=record['payload'],
                  all_other_files_unchanged=True, untouched=len(record['before'].keys() - FILES))
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    (stage/'INSTALLED.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare', action='store_true')
    action.add_argument('--smoke', type=Path)
    action.add_argument('--install', type=Path)
    args = parser.parse_args()
    if args.prepare: prepare()
    elif args.smoke: smoke(args.smoke)
    else: install(args.install)
