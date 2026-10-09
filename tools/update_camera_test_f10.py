"""Install only the two F10-fixed engines into the private Camera Edge Test.

Requires a closed game, tests a disposable copy, preserves verified rollback
engines, and checks all other installed files plus the main portable unchanged.
No archive, publication, input migration or camera-setting change. Optional
--camera-evidence proof-gates reuse for the stable-camera revision as well.
--esc-resume additionally verifies Escape, both auto-pause policies and modal
cancellation on the real English/Japanese engines before replacing them.
--edge-cover-evidence requires the paired camera/gameplay/mask checks and tests
the launcher's new reversible switch in the disposable copy only.
"""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess

from benchmark_presentation_filters import environment
from package_alpha import check_dependencies, file_hash, pe_info
from package_follow_edge_test import execute, require_local_selection
from promote_portable_beta import no_game_running
from update_guard_performance import ROOT, OWNER, stamp, snapshot, replace_with_verified_copy

TARGET = OWNER / 'release/Portable Camera Edge Test'
MAIN = OWNER / 'release/Portable Beta'
STARTER = 'Swordcraft Story 3 Beta.exe'
SOURCES = {
    'Swordcraft3CustomRendererBeta.exe': ROOT / 'build-native/Swordcraft3Translation106.exe',
    'Swordcraft3Japanese.exe': ROOT / 'build-native/Swordcraft3Japanese106.exe',
}
DLLS = ('SDL2.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')

ESC_SCRIPT = (
    '20:capture;30:menu_escape;31:menu_auto_on;33:menu_probe;38:menu_probe;'
    '40:menu_pause;41:menu_probe;42:capture;45:menu_escape;46:menu_probe;55:menu_probe;'
    '60:menu_escape;61:menu_auto_off;63:menu_probe;73:menu_probe;75:menu_pause;'
    '76:menu_probe;83:menu_probe;85:menu_close;86:menu_probe;87:menu_escape;88:menu_probe;'
    '90:menu_escape;91:menu_probe;101:menu_probe;110:menu_escape;112:menu_reset;'
    '113:menu_escape;114:menu_probe;115:menu_escape;116:menu_probe;'
    '120:menu_escape;122:menu_close;124:menu_confirm')


def verify_escape(log):
    rows = {}
    for line in log.splitlines():
        if '[runtime-menu]' not in line:
            continue
        row = {k: int(v) for k, v in re.findall(
            r'(pump|frame|cycles|manual_pause|open|confirm|audio|auto_pause)=(\d+)', line)}
        rows[row['pump']] = row
    for pump in (33, 38, 41, 63, 73, 76, 83, 86, 88, 114):
        assert rows[pump]['open'] == 1, rows[pump]
    for pump in (46, 55, 91, 101, 116):
        assert rows[pump]['open'] == rows[pump]['manual_pause'] == rows[pump]['confirm'] == 0, rows[pump]
    for pump in (41, 76, 83, 86, 88):
        assert rows[pump]['manual_pause'] == 1, rows[pump]
    assert rows[33]['auto_pause'] == rows[38]['auto_pause'] == 1
    assert rows[63]['auto_pause'] == rows[73]['auto_pause'] == 0
    for first, last in ((33, 38), (76, 83), (86, 88)):
        assert rows[first]['frame'] == rows[last]['frame']
        assert rows[first]['cycles'] == rows[last]['cycles']
    for first, last in ((46, 55), (63, 73), (91, 101)):
        assert rows[last]['frame'] > rows[first]['frame']
        assert rows[last]['cycles'] > rows[first]['cycles']
    assert rows[86]['confirm'] == 1 and rows[88]['confirm'] == rows[114]['confirm'] == 0
    assert len({row['audio'] for row in rows.values()}) == 1
    assert '[runtime-menu] event=menu_confirm pump=124' in log
    return dict(passed=True, auto_pause_and_live_menu=True, manual_pause_resumed=True,
                reset_and_close_cancelled=True, audio_preference_unchanged=True, events=rows)


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def verify_captures(folder):
    reports = sorted(folder.glob('frame-*/report.json'))
    assert len(reports) == 2, 'Expected two real export bundles: ' + str(folder)
    result = []
    for path in reports:
        report = json.loads(path.read_text())
        assert report['frame_saved'] and report['state_saved']
        assert report['host_width'] == 384 and report['layer_mask'] == 31
        for name in ('frame.png', 'state.gbas'):
            assert (path.parent / name).stat().st_size > 32
        result.append(dict(directory=str(path.parent), **report))
    return result


def main(camera_evidence=None, esc_resume=False, edge_cover_evidence=None):
    no_game_running()
    evidence = None
    cover_evidence = None
    if edge_cover_evidence is not None:
        edge_cover_evidence = edge_cover_evidence.resolve(strict=True)
        assert edge_cover_evidence.is_relative_to((ROOT / 'validation').resolve(strict=True))
        cover_evidence = json.loads(edge_cover_evidence.read_text(encoding='utf-8'))
        assert cover_evidence.get('kind') == 'battle-edge-cover-v1'
        assert all(cover_evidence.get(k) for k in ('passed', 'inputs_unchanged', 'main_unchanged', 'test_unchanged'))
        assert len(cover_evidence['cases']) == 12 and len(cover_evidence['captures']) == len(cover_evidence['transitions']) == 1
        assert all(file_hash(Path(p)) == h for p, h in cover_evidence['protected'].items()), 'Edge evidence is stale'
        for source in SOURCES.values():
            assert cover_evidence['protected'][str(source.resolve(strict=True))] == file_hash(source)
    if camera_evidence is not None:
        camera_evidence = camera_evidence.resolve(strict=True)
        assert camera_evidence.is_relative_to((ROOT / 'validation').resolve(strict=True))
        evidence = json.loads(camera_evidence.read_text(encoding='utf-8'))
        assert all(evidence.get(k) for k in ('passed', 'full_matrix', 'inputs_unchanged', 'main_unchanged', 'test_unchanged'))
        assert len(evidence['cases']) == 25 and len(evidence['transitions']) == 2
        assert all(file_hash(Path(p)) == h for p, h in evidence['protected'].items()), 'Evidence is stale'
        for source in SOURCES.values():
            assert evidence['protected'][str(source.resolve(strict=True))] == file_hash(source)
    assert TARGET.resolve(strict=True) == TARGET and MAIN.resolve(strict=True) == MAIN
    require_local_selection(TARGET)
    before, main_before = snapshot(TARGET), snapshot(MAIN)
    prefix = 'camera-cover-update-' if cover_evidence else 'camera-esc-update-' if esc_resume else 'camera-stable-update-' if evidence else 'camera-f10-update-'
    stage = ROOT / 'validation' / (prefix + stamp())
    engines, backup, test = stage / 'engines', stage / 'rollback', stage / 'test'
    engines.mkdir(parents=True)
    backup.mkdir()
    report = dict(installed=False, target=str(TARGET), backup=str(backup),
                  main_before=main_before, target_before=before, candidates={}, smoke={})
    if evidence:
        report['camera_evidence'] = dict(path=str(camera_evidence), sha256=file_hash(camera_evidence))
    if cover_evidence:
        report['edge_cover_evidence'] = dict(path=str(edge_cover_evidence), sha256=file_hash(edge_cover_evidence))
    replaced = []
    print('STAGE=' + str(stage), flush=True)
    try:
        metadata = {name: pe_info(TARGET / 'Runtime' / name) for name in DLLS}
        metadata[STARTER] = pe_info(TARGET / STARTER)
        for name, source in SOURCES.items():
            candidate = engines / name
            shutil.copyfile(source, candidate)
            original = pe_info(source)
            subprocess.run(['C:/msys64/mingw64/bin/strip.exe', '--strip-debug', str(candidate)],
                           check=True, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
            stripped = pe_info(candidate)
            assert all(original[k] == stripped[k] for k in ('loaded_sections', 'imports', 'entry_rva'))
            metadata[name] = stripped
            report['candidates'][name] = dict(source=str(source), source_sha256=file_hash(source),
                sha256=file_hash(candidate), loaded_sections_preserved=True)
        check_dependencies(metadata)
        shutil.copytree(TARGET, test)
        for name in SOURCES:
            shutil.copyfile(engines / name, test / 'Runtime' / name)
        require_local_selection(test)
        subprocess.run([str(test / STARTER), '--check'], cwd=stage, check=True,
                       timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
        neutral = stage / 'neutral.trace'
        neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
        env = environment(test, 384, neutral, audit=True)
        # Exercise the actual launcher UI with its existing automated input API.
        env.pop('SDL_VIDEODRIVER', None)
        env.pop('SDL_RENDER_DRIVER', None)
        system = os.environ.get('SystemRoot', os.environ.get('SYSTEMROOT'))
        env['PATH'] = system + '/System32;' + system
        env['GBARECOMP_STRICT_STATIC'] = '1'
        assert 'GBARECOMP_VISIBLE_DEBUGGER' not in env
        captures = stage / 'english-captures'
        env['GBARECOMP_DEBUG_CAPTURE_DIR'] = str(captures)
        env['LNG_SCRIPT'] = 'wait:8;romprobe;play'
        if cover_evidence:
            env['LNG_SCRIPT'] = ('wait:8;view:mods;builtinprobe;builtintoggle:3;builtinprobe;'
                                 'builtintoggle:3;builtinprobe;romprobe;play')
        env['GBARECOMP_ASSIST_SCRIPT'] = (
            '20:capture;30:menu_open;32:menu_pause;33:menu_probe;35:capture;40:menu_resume;41:menu_probe;'
            '50:menu_open;55:menu_close;60:menu_confirm')
        if esc_resume:
            env['GBARECOMP_ASSIST_SCRIPT'] = ESC_SCRIPT
        log = execute(test / 'Runtime/Swordcraft3CustomRendererBeta.exe',
            ['--window', '--launcher', '--save', str(test / 'Saves/f10-smoke.eep'),
             str(test / 'Runtime/game.toml')], stage, env, 'english')
        assert 'verified=1 can_play=1 patch=1 language=English (translation 1.0.6.f)' in log
        assert '[runtime-menu] event=menu_confirm' in log
        if not esc_resume:
            assert re.search(r'event=menu_probe pump=33[^\n]+manual_pause=1', log)
            assert re.search(r'event=menu_probe pump=41[^\n]+manual_pause=0', log)
        assert 'dispatch_misses=0' in (stage / 'english.out').read_text(errors='replace')
        report['smoke']['english'] = dict(launcher_play=True, clean_exit=True,
            paused_and_running_capture=True, captures=verify_captures(captures))
        if esc_resume:
            report['smoke']['english']['escape'] = verify_escape(log)
        if cover_evidence:
            choices = [int(x) for x in re.findall(r'id=battle-scenery-edge-cover enabled=(\d)', log)]
            assert len(choices) == 3 and choices == [choices[0], 1-choices[0], choices[0]], choices
            assert log.count('[launcher:builtin] saved=1') == 2
            setting = (test / 'Settings/battle-camera.ini').read_text(encoding='utf-8-sig')
            assert re.search(r'battle_edge_cover\s*=\s*' + str(choices[-1]) + r'\b', setting)
            assert '[sc3:battle-cover] preference=' + ('on' if choices[-1] else 'off') in log
            report['smoke']['english']['edge_cover_switch'] = dict(passed=True, choices=choices, saved=True)
        env.pop('LNG_SCRIPT')
        env['SDL_VIDEODRIVER'] = 'dummy'
        env['SDL_RENDER_DRIVER'] = 'software'
        env['GBARECOMP_ASSIST_SCRIPT'] = ESC_SCRIPT if esc_resume else '20:capture;40:capture'
        captures = stage / 'japanese-captures'
        env['GBARECOMP_DEBUG_CAPTURE_DIR'] = str(captures)
        bios_cfg = test / 'Settings/bios.cfg'
        bios = (bios_cfg.parent / bios_cfg.read_text(encoding='utf-8-sig').strip()).resolve(strict=True)
        assert bios.is_relative_to(test.resolve(strict=True))
        log = execute(test / 'Runtime/Swordcraft3Japanese.exe',
            ['--window', '--no-launcher', '--frames', '300' if esc_resume else '90', '--rom',
             str(test / 'ROMs/Swordcraft Story 3 Japanese.gba'), '--bios', str(bios),
             '--save', str(test / 'Saves/f10-japanese-smoke.eep'), str(test / 'Runtime/game.toml')],
            stage, env, 'japanese')
        assert 'dispatch_misses=0' in (stage / 'japanese.out').read_text(errors='replace')
        report['smoke']['japanese'] = dict(clean_exit=True, captures=verify_captures(captures))
        if esc_resume:
            report['smoke']['japanese']['escape'] = verify_escape(log)
        report['smoke'].update(passed=True, debugger_disabled=True, no_pixel_assertions=True)
        print('PASS: packaged English launcher and Japanese engine export with debugger disabled', flush=True)
        no_game_running()
        assert snapshot(TARGET) == before and snapshot(MAIN) == main_before
        for name, candidate in report['candidates'].items():
            assert file_hash(engines / name) == candidate['sha256']
            assert file_hash(SOURCES[name]) == candidate['source_sha256']
            shutil.copyfile(TARGET / 'Runtime' / name, backup / name)
            assert file_hash(backup / name) == before['Runtime/' + name]
        write_json(stage / 'REPORT.json', report)
        for name in SOURCES:
            replace_with_verified_copy(engines / name, TARGET / 'Runtime' / name)
            replaced.append(name)
        subprocess.run([str(TARGET / STARTER), '--check'], cwd=stage, check=True,
                       timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
        expected = dict(before)
        for name in SOURCES:
            expected['Runtime/' + name] = report['candidates'][name]['sha256']
        assert snapshot(TARGET) == expected, 'Unexpected changes in test installation'
        assert snapshot(MAIN) == main_before, 'Main portable changed'
        report.update(installed=True, main_unchanged=True, all_other_test_files_unchanged=True,
                      rollback_verified=True, source_hashes_preserved=True)
    except BaseException:
        for name in replaced:
            replace_with_verified_copy(backup / name, TARGET / 'Runtime' / name)
        raise
    finally:
        write_json(stage / 'REPORT.json', report)
    print('UPDATED=' + str(TARGET / STARTER), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--camera-evidence', type=Path)
    parser.add_argument('--esc-resume', action='store_true')
    parser.add_argument('--edge-cover-evidence', type=Path)
    args = parser.parse_args()
    main(args.camera_evidence, args.esc_resume, args.edge_cover_evidence)
