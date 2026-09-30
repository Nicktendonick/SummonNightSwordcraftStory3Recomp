"""Promote the built-in Guard switch into the existing main Portable Beta.

Fixed target, tiny allowlist, verified backup, no user data borrowed for smoke
tests. Never publishes or includes private game data in a release archive.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from package_alpha import file_hash, pe_info, check_dependencies
from update_guard_performance import ROOT, OWNER, snapshot, stamp, replace_with_verified_copy
from promote_portable_beta import no_game_running, provenance, require
import playtest_smooth_guard as play

TARGET = OWNER/'release/Portable Beta'
STARTER = 'Swordcraft Story 3 Beta.exe'
RETIRED = 'Swordcraft Story 3 Guard Test.exe'
ENGINES = ('Swordcraft3CustomRendererBeta.exe', 'Swordcraft3Japanese.exe')
DLLS = ('SDL2.dll', 'libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll')
FILES = {STARTER, *(f'Runtime/{n}' for n in ENGINES), 'README.md', 'Settings/guard.ini'}


def prepare():
    require(TARGET.resolve(strict=True) == TARGET, 'Unexpected target location')
    no_game_running()
    before = snapshot(TARGET)
    require('Runtime/package-manifest.json' not in before, 'Manifest-bearing installation needs separate handling')
    require('Settings/guard.ini' not in before, 'Existing preference: review before enabling it')
    stage = ROOT/'validation'/('guard-mod-release-'+stamp())
    payload, package = stage/'payload', stage/'smoke-package'
    payload.mkdir(parents=True)
    package.mkdir()
    sources, binaries = {}, {}
    for relative in sorted(FILES - {'Settings/guard.ini'}):
        source = ROOT/'packaging/portable-beta/README.md' if relative == 'README.md' else ROOT/'build-native'/Path(relative).name
        dest = payload/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        sources[str(source)] = file_hash(source)
        shutil.copyfile(source, dest)
        if relative.endswith('.exe'):
            original = pe_info(source)
            subprocess.run(['C:/msys64/mingw64/bin/strip.exe', '--strip-debug', str(dest)], check=True,
                           timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            candidate = pe_info(dest)
            for field in ('loaded_sections', 'imports', 'entry_rva'):
                require(original[field] == candidate[field], 'Stripping changed executable code: '+relative)
            binaries[Path(relative).name] = candidate
    (payload/'Settings').mkdir()
    (payload/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
    # Dependency/art/credits copy ONLY. No player inputs, settings, or save files.
    for relative in before:
        if relative.startswith(('Runtime/assets/', 'Credits/')) or relative == 'Runtime/game.toml' or relative in {'Runtime/'+n for n in DLLS}:
            dest = package/relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(TARGET/relative, dest)
            require(file_hash(dest) == before[relative], 'Dependency copy mismatch')
    for name in DLLS:
        binaries[name] = pe_info(package/'Runtime'/name)
    check_dependencies(binaries)
    for relative in FILES - {'Settings/guard.ini'}:
        dest = package/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(payload/relative, dest)
    require(snapshot(TARGET) == before, 'Main installation changed during preparation')
    report = dict(target=str(TARGET), before=before, sources=sources, payload=snapshot(payload),
                  source_state=provenance(), dependency_closure=True, pe_code_unchanged_by_strip=True)
    (stage/'PREPARED.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(stage, flush=True)


def checked(stage):
    stage = stage.resolve(strict=True)
    require(stage.parent == ROOT/'validation' and stage.name.startswith('guard-mod-release-'), 'Invalid stage')
    record = json.loads((stage/'PREPARED.json').read_text())
    require(record['target'] == str(TARGET) and TARGET.resolve(strict=True) == TARGET, 'Target identity mismatch')
    require(set(record['payload']) == FILES and snapshot(stage/'payload') == record['payload'], 'Payload changed')
    return stage, record


def boot_smoke(stage, package):
    """Actual Mods -> Play -> language handoff/reset; private inputs read-only."""
    results = []
    roms = [OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',
            OWNER/'release/swordcraft3_jp - Copy.gba']
    bios = OWNER/'gbarecomp/bios/gba_bios.bin'
    protected = {p:file_hash(p) for p in [*roms, bios]}
    for language, rom in zip(('english', 'japanese'), roms):
        for enabled in (False, True):
            run = stage/('boot-'+language+'-'+str(int(enabled)))
            (run/'Settings').mkdir(parents=True)
            (run/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = '+str(int(not enabled))+'\n', encoding='utf-8')
            (run/'Settings/rom.cfg').write_text(str(rom)+'\n', encoding='utf-8')
            (run/'Settings/bios.cfg').write_text(str(bios)+'\n', encoding='utf-8')
            trace = run/'neutral.trace'
            trace.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
            env = play.bench.environment(run, 384, trace)
            env.update(SWORDCRAFT3_SELECT_GUARD=str(int(not enabled)),
                       LNG_SCRIPT='wait:5;view:mods;builtintoggle:0;builtinprobe;romprobe;play',
                       GBARECOMP_ASSIST_SCRIPT='30:menu_open;45:menu_reset;60:menu_confirm',
                       GBARECOMP_ASSIST_SCRIPT_AFTER_RESET='30:menu_open;45:menu_close;60:menu_confirm')
            exe = package/'Runtime'/ENGINES[0]
            with (run/'stdout.log').open('wb') as stdout, (run/'stderr.log').open('wb') as stderr:
                child = subprocess.Popen([str(exe), '--window', '--launcher', '--view-width', '240',
                                          str(package/'Runtime/game.toml')], cwd=run, env=env,
                                         stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
                child.wait(timeout=35)
                require(child.returncode == 0, 'Boot/reset failed')
            log = (run/'stderr.log').read_text(errors='replace')
            preference = '[sc3:guard] preference='+('on' if enabled else 'off')
            expected = 2 if language == 'english' else 3  # parent -> JP child -> reset child
            require(log.count(preference) == expected, 'Preference lost across Play/handoff/reset')
            require(log.count('Clean reset:') == 1, 'Reset not exercised')
            require('[launcher:builtin] saved=1' in log, 'Mods action not committed')
            require('[launcher:builtin] id=select-guard enabled='+str(int(enabled)) in log, 'Wrong UI choice')
            require('can_play=1' in log, 'Play not verified')
            require(log.count('Hold Select to Guard; authenticated=1') == (2 if enabled else 0),
                    'Wrong hook activation after cold boot/reset')
            require(all(file_hash(p) == h for p,h in protected.items()), 'Original input changed')
            results.append(dict(language=language, enabled=enabled, play=True, reset=True,
                                language_handoff=language == 'japanese', environment_override_ignored=True))
            print('PASS: Play/reset '+language+' Guard='+str(int(enabled)), flush=True)
    return results


def smoke(stage):
    stage, record = checked(stage)
    no_game_running()
    require(snapshot(TARGET) == record['before'], 'Main install changed')
    package = stage/'smoke-package'
    protected = snapshot(package)
    result = dict(passed=False, cases=[])
    system = os.environ.get('SYSTEMROOT') or os.environ.get('SystemRoot')
    require(bool(system), 'Missing system directory')
    try:
        for label, commands, expected in [
            ('fresh-enable', 'builtinprobe;builtintoggle:0;builtinprobe', [0, 1]),
            ('reopen-disable', 'builtinprobe;builtintoggle:0;builtinprobe', [1, 0]),
            ('reopen-off', 'builtinprobe', [0]),
            ('reopen-enable', 'builtinprobe;builtintoggle:0;builtinprobe', [0, 1]),
        ]:
            env = {k:v for k,v in os.environ.items() if not k.startswith(('SWORDCRAFT3_', 'GBARECOMP_', 'SDL_', 'LNG_'))}
            env['PATH'] = os.path.join(system, 'System32')+';'+system
            env['LNG_SCRIPT'] = 'wait:5;view:mods;wait:5;'+commands+';wait:5;quit'
            before_logs = set((package/'Captures').glob('*/session.log'))
            child = subprocess.Popen([str(package/STARTER)], cwd=system, env=env,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            child.wait(timeout=30)
            require(child.returncode == 0, 'Starter failed: '+label)
            logs = set((package/'Captures').glob('*/session.log')) - before_logs
            require(len(logs) == 1, 'Expected one session log')
            log = next(iter(logs)).read_text(errors='replace')
            observed = [int(line.rsplit('=',1)[1]) for line in log.splitlines()
                        if line.startswith('[launcher:builtin] id=select-guard enabled=')]
            require(observed == expected, 'Wrong persisted toggle values: '+repr(observed))
            if 'builtintoggle' in commands:
                require('[launcher:builtin] saved=1' in log, 'Toggle did not save')
            require(('select_guard = '+str(expected[-1])) in (package/'Settings/guard.ini').read_text(), 'Preference not persisted')
            result['cases'].append(dict(name=label, values=observed, reduced_path=True))
            print('PASS: '+label, flush=True)
        # Real pause/resume/graphics/state menu using the exact staged engine.
        play.EXE = package/'Runtime'/ENGINES[0]
        trace = stage/'neutral.trace'
        trace.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
        result['runtime_menu'] = play.menu_playtest(stage/'menu-smoke', play.bench.DEFAULT_STATE, trace)
        result['boot'] = boot_smoke(stage, package)
        for folder in ('ROMs', 'BIOS', 'Saves', 'Save States'):
            require(not any(p.is_file() for p in (package/folder).rglob('*')), 'User input unexpectedly borrowed')
        require(all(file_hash(package/p) == h for p,h in protected.items()), 'Smoke modified program payload')
        require(snapshot(TARGET) == record['before'], 'Smoke touched main installation')
        result.update(passed=True, main_untouched=True, payload=record['payload'])
    finally:
        (stage/'SMOKE.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(stage/'SMOKE.json', flush=True)


def install(stage, regression):
    stage, record = checked(stage)
    no_game_running()
    require(snapshot(TARGET) == record['before'], 'Main install changed since preparation')
    smoke_result = json.loads((stage/'SMOKE.json').read_text())
    require(smoke_result['passed'] and smoke_result['payload'] == record['payload'], 'Smoke evidence absent/stale')
    guard = json.loads(regression.read_text())
    require(guard['passed'] and guard['inputs_unchanged'] and guard['executables_unchanged'], 'Guard regression failed')
    for name in ENGINES:
        path = stage/'smoke-package/Runtime'/name
        require(guard['executable_sha256'].get(str(path)) == record['payload']['Runtime/'+name] == file_hash(path),
                'Regression did not test this exact engine')
    require(all(file_hash(Path(p)) == h for p,h in record['sources'].items()), 'Build sources changed')
    backup = OWNER/'release'/('Portable Beta Guard Rollback '+stamp())
    backup.mkdir()
    for relative in (FILES | {RETIRED}) & record['before'].keys():
        dest = backup/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(TARGET/relative, dest)
        require(file_hash(dest) == record['before'][relative], 'Backup failed')
    expected = dict(record['before'])
    expected.update(record['payload'])
    expected.pop(RETIRED, None)
    update = dict(record, installed=False, backup=str(backup), expected=expected,
                  regression=str(regression), untouched=len(record['before'].keys() - FILES - {RETIRED}))
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(update, indent=2), encoding='utf-8')
    changed = []
    retired = False
    try:
        no_game_running()
        require(snapshot(TARGET) == record['before'], 'Target changed during backup')
        for relative in sorted(FILES):
            replace_with_verified_copy(stage/'payload'/relative, TARGET/relative)
            changed.append(relative)
        if (TARGET/RETIRED).exists():
            # Exact verified file, recoverable: move into this update's backup.
            os.replace(TARGET/RETIRED, backup/RETIRED)
            retired = True
        subprocess.run([str(TARGET/STARTER), '--check'], cwd=stage, check=True, timeout=15,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        require(snapshot(TARGET) == expected, 'Unexpected installation change')
    except BaseException:
        if retired:
            replace_with_verified_copy(backup/RETIRED, TARGET/RETIRED)
        for relative in reversed(changed):
            if relative in record['before']:
                replace_with_verified_copy(backup/relative, TARGET/relative)
            else:
                recovery = backup/'recovered-added-files'/relative
                recovery.parent.mkdir(parents=True, exist_ok=True)
                os.replace(TARGET/relative, recovery)
        raise
    update.update(installed=True, all_other_files_unchanged=True)
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(update, indent=2), encoding='utf-8')
    (stage/'INSTALLED.json').write_text(json.dumps(update, indent=2), encoding='utf-8')
    print(json.dumps({k:update[k] for k in ('installed', 'target', 'backup', 'untouched', 'all_other_files_unchanged')}, indent=2), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare', action='store_true')
    action.add_argument('--smoke', type=Path)
    action.add_argument('--install', type=Path)
    parser.add_argument('--regression', type=Path)
    args = parser.parse_args()
    if args.prepare: prepare()
    elif args.smoke: smoke(args.smoke)
    else:
        require(args.regression is not None, 'Installation requires exact-binary regression evidence')
        install(args.install, args.regression)
