"""Prepare a separate PRIVATE Follow + edge stops portable, never update main.

The clean program recipe receives private copies of the owner's settings,
inputs and saves. Diagnostics/caches outside Mods are not copied. No archive,
publication, deletion, or replacement of an existing test folder is supported.
"""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from package_alpha import check_dependencies, file_hash, pe_info
from package_portable_release import DATA_DIRS, GAME, JAPANESE, STARTER, recipe
from promote_portable_beta import no_game_running
from update_guard_performance import ROOT, OWNER, snapshot, stamp

MAIN = OWNER / 'release/Portable Beta'
TARGET = OWNER / 'release/Portable Camera Edge Test'
STRIP = Path('C:/msys64/mingw64/bin/strip.exe')
PRIVATE_DIRS = ('BIOS', 'ROMs', 'Mods', 'Settings', 'Saves', 'Save States')
ENGLISH_ENGINE = ROOT / 'build-native/Swordcraft3Translation106.exe'
ENGLISH_ROM = ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
JAPANESE_ROM = OWNER.parent / 'Swordcraft Story 3 rom files/swordcraft3_jp.gba'
PATCH = OWNER.parent / 'Swordcraft Story 3 rom files/Hajimari_no_Ishi_v1.0.6.f.bps'
ENGLISH_SHA256 = '0956c4bea57201592a82ff2b72ecc57641729aaed85492d3bcd6f87c99b8c2dd'
JAPANESE_SHA1 = '3f5253fcf57e07ce52472bd29a61d16b98a12376'
JP_RELATIVE = 'ROMs/Swordcraft Story 3 Japanese.gba'
EN_RELATIVE = 'ROMs/Swordcraft Story 3 English 1.0.6f.gba'
CHANGED_SETTINGS = {'Settings/battle-camera.ini', 'Settings/launcher.ini', 'Settings/rom.cfg'}


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def validation_proof(path):
    """Require the completed matrix for the exact unstripped package engine."""
    path = path.resolve(strict=True)
    assert path.is_relative_to(ROOT / 'validation'), 'Evidence must stay in project validation'
    report = json.loads(path.read_text(encoding='utf-8'))
    for key in ('passed', 'full_matrix', 'inputs_unchanged', 'main_unchanged'):
        assert report.get(key) is True, 'Required matrix gate failed: ' + key
    assert Path(report['candidate']).resolve(strict=True) == ENGLISH_ENGINE.resolve(strict=True)
    assert report['candidate_sha256'] == file_hash(ENGLISH_ENGINE), 'Matrix tested a different engine'
    assert Path(report['candidate_rom']).resolve(strict=True) == ENGLISH_ROM.resolve(strict=True)
    assert report['candidate_rom_sha256'] == ENGLISH_SHA256 == file_hash(ENGLISH_ROM)
    assert all(file_hash(Path(p)) == h for p, h in report['source_hashes'].items()), 'Matrix inputs changed'
    return dict(path=str(path), sha256=file_hash(path), candidate_sha256=report['candidate_sha256'],
                candidate_rom_sha256=report['candidate_rom_sha256'], full_matrix=True)


def private_input(source, relative, payload, sources):
    """Copy a named private input, refusing to overwrite different player bytes."""
    assert source.is_file() and not source.is_symlink(), source
    sources[str(source)] = file_hash(source)
    target = payload / relative
    if target.exists():
        assert file_hash(target) == sources[str(source)], 'Existing private input conflicts: ' + relative
    else:
        shutil.copyfile(source, target)
    assert file_hash(target) == sources[str(source)]


def require_local_selection(package):
    """Refuse selections that could use main's writable data after relocation."""
    for relative in ('Settings/rom.cfg', 'Settings/bios.cfg'):
        path = package / relative
        selected = Path(path.read_text(encoding='utf-8-sig').strip())
        if selected.is_absolute():
            raise ValueError('Private source selection must be portable/relative: ' + relative)
        resolved = (path.parent / selected).resolve(strict=True)
        if not resolved.is_relative_to(package.resolve(strict=True)) or not resolved.is_file():
            raise ValueError('Selection escapes private package: ' + relative)
    config = configparser.ConfigParser(interpolation=None)
    config.read(package / 'Settings/launcher.ini', encoding='utf-8-sig')
    selected = config.get('Launcher', 'rom_patch_path', fallback='').strip()
    if selected:
        path = Path(selected)
        if path.is_absolute() or not (package / 'Settings' / path).resolve(strict=True).is_relative_to(package.resolve(strict=True)):
            raise ValueError('Patch selection must stay inside private package')


def prepare(validation_report):
    no_game_running()
    proof = validation_proof(validation_report)
    assert ENGLISH_ROM.stat().st_size == JAPANESE_ROM.stat().st_size == 0x2000000
    assert hashlib.sha1(JAPANESE_ROM.read_bytes()).hexdigest() == JAPANESE_SHA1
    assert MAIN.resolve(strict=True) == MAIN
    before = snapshot(MAIN)
    stage = ROOT / 'validation' / ('camera-edge-' + stamp())
    payload = stage / 'payload'
    payload.mkdir(parents=True, exist_ok=False)
    sources, binaries = {}, {}
    for source, relative in recipe():
        if relative == 'Runtime/' + GAME:
            assert source.resolve(strict=True) == ENGLISH_ENGINE.resolve(strict=True)
        elif relative == 'Runtime/' + JAPANESE:
            assert source.resolve(strict=True) == (ROOT / 'build-native/Swordcraft3Japanese106.exe').resolve(strict=True)
        assert source.is_file() and not source.is_symlink(), source
        target = payload / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        sources[str(source)] = file_hash(source)
        shutil.copyfile(source, target)
        if target.suffix.lower() in ('.exe', '.dll'):
            before_pe = pe_info(source)
            subprocess.run([str(STRIP), '--strip-debug', str(target)], check=True,
                           timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
            after_pe = pe_info(target)
            assert all(before_pe[k] == after_pe[k] for k in ('loaded_sections', 'imports', 'entry_rva'))
            binaries[target.name] = after_pe
    check_dependencies(binaries)
    for directory in DATA_DIRS:
        destination = payload / directory
        if directory in PRIVATE_DIRS and (MAIN / directory).is_dir():
            shutil.copytree(MAIN / directory, destination)
        else:
            destination.mkdir(exist_ok=True)
    # A test-only override; source preferences are never migrated in place.
    (payload / 'Settings/battle-camera.ini').write_text('[Launcher]\nbattle_camera_mode = 2\n', encoding='utf-8')
    private_input(JAPANESE_ROM, JP_RELATIVE, payload, sources)
    private_input(ENGLISH_ROM, EN_RELATIVE, payload, sources)
    private_input(PATCH, 'Mods/' + PATCH.name, payload, sources)
    # Use the normal Japanese-plus-BPS workflow. Old patches/cache remain as
    # inert private copies, not selections; 1.0.6 is the only English target.
    (payload / 'Settings/rom.cfg').write_text('../' + JP_RELATIVE + '\n', encoding='utf-8')
    ini_path = payload / 'Settings/launcher.ini'
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(ini_path, encoding='utf-8-sig')
    if not ini.has_section('Launcher'):
        ini.add_section('Launcher')
    ini.set('Launcher', 'rom_patch_enabled', '1')
    ini.set('Launcher', 'rom_patch_path', '../Mods/' + PATCH.name)
    with ini_path.open('w', encoding='utf-8') as stream:
        ini.write(stream)
    old_battery = payload / 'Saves/english-1.0.5f.eep'
    new_battery = payload / 'Saves/english-1.0.6f.eep'
    battery = dict(copied=False, existing_106_preserved=new_battery.exists())
    if old_battery.is_file() and not new_battery.exists():
        assert old_battery.stat().st_size == 8192, 'Unexpected previous battery-save size'
        shutil.copyfile(old_battery, new_battery)
        assert file_hash(new_battery) == file_hash(old_battery)
        battery.update(copied=True, source='Saves/english-1.0.5f.eep',
                       target='Saves/english-1.0.6f.eep', sha256=file_hash(new_battery),
                       private_clone_only=True, user_progress_not_playtested=True)
    require_local_selection(payload)
    (payload / 'CAMERA-EDGE-TEST.txt').write_text(
        'PRIVATE LOCAL EXPERIMENT - not a release archive.\n\n'
        'Launch Swordcraft Story 3 Beta.exe in THIS folder.\n'
        'Battle camera starts at Follow + edge stops. Switch among Current,\n'
        'Bounded overview and Follow + edge stops in Mods or Esc > Graphics.\n'
        'This folder contains independent copies of your settings and saves.\n'
        'Changes made here are not copied back to Portable Beta.\n'
        'English patch: 1.0.6.f. Turn the patch off to play original Japanese.\n'
        'If present, the 1.0.5 battery save is copied once to a new 1.0.6 save.\n'
        'The original save is kept; verify Continue before relying on the copy.\n'
        'Old save states are preserved in their old folders, not converted.\n'
        'At 12:5 the scene has fixed 19-pixel side frames and no zoom.\n'
        'No scene support is added: unsupported arenas retain native fallback.\n'
        'State/source checks do not certify visual quality or every spell.\n', encoding='utf-8')
    assert snapshot(MAIN) == before, 'Main install changed during preparation'
    assert all(file_hash(Path(p)) == h for p, h in sources.items())
    copied = {p: h for p, h in before.items() if p.split('/')[0] in PRIVATE_DIRS and p not in CHANGED_SETTINGS}
    assert all(file_hash(payload / p) == h for p, h in copied.items())
    record = dict(private_only=True, publishable=False, main=str(MAIN), main_before=before,
                  source_hashes=sources, payload=snapshot(payload), copied_player_data=copied,
                  camera_mode=2, dependency_closure=True, loaded_sections_unchanged=True,
                  validation=proof, english_patch='1.0.6.f', private_battery_copy=battery,
                  user_save_states_converted=False)
    write_json(stage / 'PREPARED.json', record)
    print('STAGE=' + str(stage), flush=True)
    return stage


def checked(stage):
    stage = stage.resolve(strict=True)
    assert stage.parent == ROOT / 'validation' and stage.name.startswith('camera-edge-')
    record = json.loads((stage / 'PREPARED.json').read_text(encoding='utf-8'))
    assert record['private_only'] and not record['publishable']
    assert snapshot(stage / 'payload') == record['payload'], 'Prepared payload changed'
    assert snapshot(MAIN) == record['main_before'], 'Main install changed externally'
    assert all(file_hash(Path(p)) == h for p, h in record['source_hashes'].items()), 'Build sources changed'
    assert validation_proof(Path(record['validation']['path'])) == record['validation'], 'Matrix evidence changed'
    return stage, record


def camera_probes(log):
    """Parse actual launcher model probes, never infer checkbox state from pixels."""
    rows = re.findall(r'\[launcher:builtin\] id=(bounded-battle-camera|follow-edge-battle-camera) enabled=([01])', log)
    assert len(rows) % 2 == 0
    values = []
    for n in range(0, len(rows), 2):
        pair = dict(rows[n:n + 2])
        assert set(pair) == {'bounded-battle-camera', 'follow-edge-battle-camera'}
        values.append((int(pair['bounded-battle-camera']), int(pair['follow-edge-battle-camera'])))
    return values


def execute(exe, args, run, env, label):
    """Allow first-run OpenGL warmup; timeout only this owned process tree."""
    with (run / (label + '.out')).open('wb') as out, (run / (label + '.err')).open('wb') as err:
        child = subprocess.Popen([str(exe), *args], cwd=run, env=env, stdout=out, stderr=err,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=120)
        except subprocess.TimeoutExpired:
            # Language handoff and reset create children while the owned
            # parent waits. Do not terminate only the parent and orphan them.
            # /PID is our still-live Popen PID; no name-wide/user-game kill.
            if child.poll() is None:
                system = Path(os.environ.get('SystemRoot') or os.environ['SYSTEMROOT'])
                cleanup = subprocess.run([str(system / 'System32/taskkill.exe'),
                                          '/PID', str(child.pid), '/T', '/F'],
                                         capture_output=True, text=True, timeout=20,
                                         creationflags=subprocess.CREATE_NO_WINDOW)
                (run / (label + '.timeout.txt')).write_text(
                    cleanup.stdout + cleanup.stderr, encoding='utf-8')
                child.wait(timeout=20)
            raise
    assert child.returncode == 0, label + ' process failed; inspect ' + str(run)
    return (run / (label + '.err')).read_text(errors='replace')


def smoke(stage):
    """Exercise actual launcher in a disposable copy, never in delivered saves."""
    from benchmark_presentation_filters import environment
    no_game_running()
    stage, record = checked(stage)
    # Each attempt gets fresh disposable data and logs. Preserve failed-run
    # evidence rather than deleting a previous test tree to retry.
    run = stage / ('smoke-' + stamp()[-4:])
    run.mkdir(exist_ok=False)
    test = run / 'test'
    shutil.copytree(stage / 'payload', test)
    neutral = run / 'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
    env = environment(test, 384, neutral, True)
    env.pop('SDL_VIDEODRIVER', None)
    env.pop('SDL_RENDER_DRIVER', None)
    system = os.environ.get('SystemRoot', os.environ.get('SYSTEMROOT'))
    env['PATH'] = system + '/System32;' + system
    result = dict(passed=False, payload=record['payload'], smoke_directory=str(run))
    try:
        subprocess.run([str(test / STARTER), '--check'], cwd=stage, env=env,
                       check=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        env['LNG_SCRIPT'] = ('wait:8;romprobe;romfile:' + str(test / JP_RELATIVE) +
                            ';patchfile:' + str(test / 'Mods' / PATCH.name) +
                            ';romprobe;patchtoggle;romprobe;patchtoggle;romprobe;'
                            'view:mods;builtinprobe;builtintoggle:1;builtinprobe;'
                            'builtintoggle:2;builtinprobe;builtintoggle:2;builtinprobe;'
                            'builtintoggle:2;builtinprobe;wait:3;quit')
        log = execute(test / 'Runtime' / GAME,
                      ['--window', '--launcher', str(test / 'Runtime/game.toml')], run, env, 'camera-launcher')
        assert 'verified=1 can_play=1 patch=1 language=English (translation 1.0.6.f)' in log
        assert 'verified=1 can_play=1 patch=0 language=Japanese (original)' in log
        assert 'preference=follow-edge' in log, 'Mode 2 was not loaded by the actual launcher'
        assert camera_probes(log) == [(0, 1), (1, 0), (0, 1), (0, 0), (0, 1)], 'Camera choices lost mutual exclusion'
        assert len(re.findall(r'\[launcher:builtin\] saved=1', log)) == 4
        result['real_launcher_transitions'] = ['Follow', 'Bounded', 'Follow', 'Current', 'Follow']
        result['ui_patch_import_toggle'] = True
        env['LNG_SCRIPT'] = 'wait:8;romprobe;view:mods;builtinprobe;wait:3;play'
        # --frames is a preboot bypass even beside --launcher. Open the real
        # launcher, press PLAY through its model, then close via the actual
        # Esc confirmation path once runtime input pumps are active.
        env['GBARECOMP_ASSIST_SCRIPT'] = '20:menu_open;25:menu_probe;30:menu_close;35:menu_confirm'
        log = execute(test / 'Runtime' / GAME,
                      ['--window', '--launcher', '--save',
                       str(test / 'Saves/smoke-launcher.eep'), str(test / 'Runtime/game.toml')], run, env, 'camera-reopen')
        assert camera_probes(log) == [(0, 1)] and 'preference=follow-edge' in log
        assert 'verified=1 can_play=1 patch=1 language=English (translation 1.0.6.f)' in log
        cache = test / 'Mods/rom-patches/6753a22a096b8adaa3a869333b99fcfe29ba1fec.gba'
        assert cache.is_file() and file_hash(cache) == ENGLISH_SHA256, 'UI did not generate the verified 1.0.6 image'
        assert 'dispatch_misses=0' in (run / 'camera-reopen.out').read_text(errors='replace')
        assert '[sc3:language] English (translation 1.0.6.f)' in log
        assert '[runtime-menu] event=menu_confirm' in log, 'PLAY did not exit through the game menu'
        result['camera_launcher_persistence'] = True
        env.pop('LNG_SCRIPT')
        # Cold boot from the opposite language engine to exercise sibling
        # handoff, then the real Esc graphics actions and a clean reset.
        bios_cfg = test / 'Settings/bios.cfg'
        bios = (bios_cfg.parent / bios_cfg.read_text(encoding='utf-8-sig').strip()).resolve(strict=True)
        result['languages'] = []
        for language, entry, rom in (('english', JAPANESE, cache), ('japanese', GAME, test / JP_RELATIVE)):
            env['GBARECOMP_ASSIST_SCRIPT'] = ('120:menu_open;125:menu_probe;130:menu_preset=2;'
                '135:menu_screen=3;140:menu_resume;550:menu_open;555:menu_reset;560:menu_confirm')
            env['GBARECOMP_ASSIST_SCRIPT_AFTER_RESET'] = '60:menu_open;65:menu_probe;70:menu_close;75:menu_confirm'
            args = ['--window', '--no-launcher', '--rom', str(rom), '--bios', str(bios),
                    '--save', str(test / 'Saves' / ('smoke-' + language + '.eep')), str(test / 'Runtime/game.toml')]
            log = execute(test / 'Runtime' / entry, args, run, env, language)
            stdout = (run / (language + '.out')).read_text(errors='replace')
            dispatch = re.findall(r'dispatch_misses=(\d+)', stdout)
            assert len(dispatch) >= 2 and all(int(value) == 0 for value in dispatch), stdout[-3000:]
            assert 'Clean reset:' in log and '[runtime-preset] index=2 name=Soft & Smooth' in log
            assert 'authenticated=1' in log and 'preference=follow-edge' in log
            result['languages'].append(dict(language=language, cold_boot=True, reset=True,
                sibling_handoff=True, guard_authenticated=True, no_dispatch_misses=True,
                isolated_smoke_save=True))
        assert snapshot(MAIN) == record['main_before']
        assert snapshot(stage / 'payload') == record['payload']
        assert all(file_hash(Path(p)) == h for p, h in record['source_hashes'].items())
        require_local_selection(test)
        result.update(passed=True, real_launcher_mode2=True, private_sources_unchanged=True,
                      main_unchanged=True, inputs_unchanged=True, payload_unchanged=True,
                      validation=record['validation'])
    finally:
        write_json(run / 'SMOKE.json', result)
        write_json(stage / 'SMOKE.json', result)
    print('PASS: 1.0.6 patch import, camera choices/restart, Japanese/English boot/reset; main untouched', flush=True)


def deliver(stage):
    no_game_running()
    stage, record = checked(stage)
    smoke_record = json.loads((stage / 'SMOKE.json').read_text(encoding='utf-8'))
    assert smoke_record['passed'] and smoke_record['payload'] == record['payload']
    assert smoke_record['validation'] == record['validation']
    assert all(smoke_record.get(key) is True for key in ('main_unchanged', 'inputs_unchanged', 'payload_unchanged'))
    assert TARGET.parent.resolve(strict=True) == OWNER / 'release'
    assert not TARGET.exists(), 'Existing test folder is preserved; choose a reviewed new name in the script'
    shutil.copytree(stage / 'payload', TARGET)
    assert snapshot(TARGET) == record['payload'], 'Delivered copy differs; main remains untouched'
    assert snapshot(MAIN) == record['main_before'], 'Main install changed externally'
    result = dict(delivered=True, private_only=True, target=str(TARGET), stage=str(stage),
                  main_unchanged=True, independent_saves=True, files=record['payload'],
                  validation=record['validation'], english_patch='1.0.6.f',
                  private_battery_copy=record['private_battery_copy'], user_save_states_converted=False)
    write_json(stage / 'DELIVERED.json', result)
    print('DELIVERED=' + str(TARGET), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'smoke', 'deliver'))
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--validation-report', type=Path, help='Completed full matrix REPORT.json for this exact 1.0.6 engine')
    args = parser.parse_args()
    if args.action == 'prepare':
        assert args.validation_report, '--validation-report is required for prepare'
        prepare(args.validation_report)
    else:
        assert args.stage, '--stage is required'
        (smoke if args.action == 'smoke' else deliver)(args.stage)
