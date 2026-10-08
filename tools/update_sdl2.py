"""Validate SDL2 in private copies, then optionally install only reviewed files.

No archive/publication or system-tool update. Uses state/input/filter counters,
not framebuffer assertions. Preserves rollback bytes and all player files.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
from types import SimpleNamespace

import benchmark_presentation_filters as bench
from package_alpha import check_dependencies, file_hash, pe_info
from package_follow_edge_test import execute, require_local_selection
from package_portable_release import verify_runtime_dll
from promote_portable_beta import no_game_running
from update_camera_test_f10 import ESC_SCRIPT, verify_escape
from update_guard_performance import ROOT, OWNER, snapshot, stamp

TARGET = OWNER / 'release/Portable UI Test'
STRIP = Path('C:/msys64/mingw64/bin/strip.exe')
ENGINES = {'Swordcraft3CustomRendererBeta.exe': 'Swordcraft3Translation106.exe',
           'Swordcraft3Japanese.exe': 'Swordcraft3Japanese106.exe'}
NOTICE = 'Runtime/notices/SDL2-LICENSE.txt'
REPLACEMENTS = [*(f'Runtime/{name}' for name in ENGINES), 'Runtime/SDL2.dll', NOTICE]


def save(stage, report):
    (stage / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


def local_copy(destination):
    destination.mkdir()
    for directory in ('Runtime', 'Credits', 'BIOS', 'ROMs', 'Mods', 'Settings'):
        shutil.copytree(TARGET / directory, destination / directory)
    for directory in ('Saves', 'Save States', 'Logs', 'Captures'):
        (destination / directory).mkdir()
    shutil.copyfile(TARGET / 'Swordcraft Story 3 Beta.exe', destination / 'Swordcraft Story 3 Beta.exe')
    require_local_selection(destination)


def prepare():
    no_game_running()
    require_local_selection(TARGET)
    stage = ROOT / 'validation' / ('sdl2-' + stamp())
    stage.mkdir()
    print('STAGE=' + str(stage), flush=True)
    report = dict(passed=False, installed=False, target_before=snapshot(TARGET),
                  sources={}, candidate={}, runs=[], ui={})
    for relative in REPLACEMENTS:
        backup = stage / 'rollback' / relative
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(TARGET / relative, backup)
    local_copy(stage / 'baseline')
    local_copy(stage / 'candidate')
    for relative in REPLACEMENTS:
        name = Path(relative).name
        source = (ROOT / 'packaging/portable-beta/notices/SDL2-LICENSE.txt' if relative == NOTICE
                  else ROOT / 'build-native' / ENGINES.get(name, name))
        report['sources'][str(source)] = file_hash(source)
        destination = stage / 'candidate' / relative
        shutil.copyfile(source, destination)
        if name.endswith('.exe'):
            before = pe_info(source)
            subprocess.run([str(STRIP), '--strip-debug', str(destination)], check=True,
                           creationflags=subprocess.CREATE_NO_WINDOW, timeout=60)
            after = pe_info(destination)
            assert all(before[k] == after[k] for k in ('imports', 'loaded_sections', 'entry_rva'))
        report['candidate'][relative] = file_hash(destination)
    candidate = stage / 'candidate'
    dll = candidate / 'Runtime/SDL2.dll'
    verify_runtime_dll(dll, STRIP.parent)
    library = ctypes.CDLL(str(dll))
    version = (ctypes.c_uint8 * 3)()
    library.SDL_GetVersion(ctypes.byref(version))
    report['sdl_version'] = '.'.join(str(v) for v in version)
    assert report['sdl_version'] == '2.32.10'
    binaries = {p.name: pe_info(p) for p in (candidate / 'Runtime').iterdir()
                if p.suffix.lower() in ('.exe', '.dll')}
    check_dependencies(binaries)
    report['dependency_closure'] = {name: item['imports'] for name, item in binaries.items()}
    save(stage, report)
    return stage


def test(stage, report):
    no_game_running()
    assert snapshot(TARGET) == report['target_before']
    assert all(file_hash(Path(p)) == h for p, h in report['sources'].items())
    candidate = stage / 'candidate'
    neutral = stage / 'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n', encoding='ascii')
    env = bench.environment(candidate, 384, neutral, audit=False)
    system = os.environ.get('SystemRoot') or os.environ['SYSTEMROOT']
    env['PATH'] = system + '/System32;' + system
    env['GBARECOMP_STRICT_STATIC'] = '1'
    env['GBARECOMP_ASSIST_SCRIPT'] = ESC_SCRIPT.replace('20:capture;', '').replace('42:capture;', '')
    env.pop('SWORDCRAFT3_BATTLE_HUD_BORDERS', None)
    navigation = ';'.join('view:' + p + ';wait:3;uiprobe' for p in
                          ('dashboard', 'graphics', 'audio', 'controller', 'assist_tools', 'mods', 'credits'))
    env['LNG_SCRIPT'] = 'wait:8;' + navigation + ';view:mods;builtintoggle:0;builtintoggle:0;romprobe;play'
    args = ['--window', '--launcher', '--save', str(candidate / 'Saves/ui-smoke.eep'),
            str(candidate / 'Runtime/game.toml')]
    for language in ('english', 'japanese'):
        if language == 'japanese':
            env['LNG_SCRIPT'] = 'wait:8;view:mods;patchtoggle;romprobe;play'
        log = execute(candidate / 'Runtime/Swordcraft3CustomRendererBeta.exe', args, stage, env, language)
        assert 'dispatch_misses=0' in (stage / (language + '.out')).read_text(errors='replace')
        expected = 'English (translation 1.0.6.f)' if language == 'english' else 'Japanese'
        assert 'language=' + expected in log
        if language == 'english':
            for page in ('Dashboard', 'Graphics', 'Audio', 'Controller', 'Assist Tools', 'Mods', 'Credits'):
                assert '[launcher:ui] organized=1 view=' + page + ' ' in log
        report['ui'][language] = verify_escape(log)
        # audio_enabled() requires a successfully opened audio device. This
        # short boot/menu test ends before the periodic audio probe is due.
        assert all(row['audio'] == 1 for row in report['ui'][language]['events'].values())
        save(stage, report)
        print('PASS UI / pause / resume: ' + language, flush=True)
    rom = ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
    state = ROOT / 'validation/translation106-input-audit-1790824852616961300/diagnostic-fixtures/arena-03/combat-ready.gbas'
    bios = candidate / 'BIOS/gba_bios.bin'
    protected = {str(p): file_hash(p) for p in (rom, state, bios)}
    for width in (240, 384):
        for mode in bench.MODES:
            for variant in ('baseline', 'candidate'):
                options = SimpleNamespace(exe=stage / variant / 'Runtime/Swordcraft3CustomRendererBeta.exe',
                                          frames=180, warmup_frames=60, screen='raw', strength=35,
                                          bios=bios, rom=rom, state=state)
                run = stage / f'{variant}-{width}-{mode[0]}'
                result = bench.execute(options, run, width, mode, neutral)
                result['variant'] = variant
                report['runs'].append(result)
                save(stage, report)
                print(f'PASS {variant} {width} {mode[0]}: {result["fps"]:.2f} fps', flush=True)
    assert all(file_hash(Path(p)) == h for p, h in protected.items())
    assert snapshot(TARGET) == report['target_before']
    assert all(file_hash(Path(p)) == h for p, h in report['sources'].items())
    assert all(file_hash(candidate / p) == h for p, h in report['candidate'].items())
    report.update(passed=True, private_inputs_unchanged=True, player_files_unchanged=True,
                  no_pixel_assertions=True, physical_controller_tested=False)
    save(stage, report)
    print('PASS: 24 presentation/audio cases and both language launch paths', flush=True)


def recheck(stage, report):
    """Longer reverse-order pacing check; not a weak-PC performance promise."""
    no_game_running()
    assert report['passed'] and not report.get('pacing_recheck')
    assert snapshot(TARGET) == report['target_before']
    results = []
    for mode in (bench.MODES[0], bench.MODES[-1]):
        for variant in ('candidate', 'baseline'):
            options = SimpleNamespace(exe=stage / variant / 'Runtime/Swordcraft3CustomRendererBeta.exe',
                frames=600, warmup_frames=120, screen='raw', strength=35,
                bios=stage / 'candidate/BIOS/gba_bios.bin',
                rom=ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba',
                state=ROOT / 'validation/translation106-input-audit-1790824852616961300/diagnostic-fixtures/arena-03/combat-ready.gbas')
            result = bench.execute(options, stage / f'recheck-{variant}-{mode[0]}', 384, mode, stage / 'neutral.trace')
            result['variant'] = variant
            results.append(result)
            print(f'RECHECK {variant} {mode[0]}: {result["fps"]:.2f} fps, p99 {result["gap_ms_p99"]:.2f} ms', flush=True)
    assert snapshot(TARGET) == report['target_before']
    report['pacing_recheck'] = results
    save(stage, report)


def install(stage, report):
    no_game_running()
    assert report['passed'] and not report['installed']
    assert report.get('performance_approved') is True, 'Performance approval is required before promotion'
    assert snapshot(TARGET) == report['target_before'], 'Playable copy changed since testing'
    for relative in REPLACEMENTS:
        assert file_hash(stage / 'candidate' / relative) == report['candidate'][relative]
        assert file_hash(stage / 'rollback' / relative) == report['target_before'][relative]
    replaced = []
    try:
        for relative in REPLACEMENTS:
            replaced.append(relative)
            shutil.copyfile(stage / 'candidate' / relative, TARGET / relative)
        expected = dict(report['target_before'])
        expected.update(report['candidate'])
        assert snapshot(TARGET) == expected, 'Unexpected install change'
        subprocess.run([str(TARGET / 'Swordcraft Story 3 Beta.exe'), '--check'],
                       cwd=stage, check=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
    except BaseException:
        for relative in replaced:
            shutil.copyfile(stage / 'rollback' / relative, TARGET / relative)
        raise
    report['installed'] = True
    report['other_files_unchanged'] = len(expected) - len(REPLACEMENTS)
    save(stage, report)
    print('INSTALLED: SDL2, both language engines, SDL notice; other files unchanged', flush=True)


def isolate(stage, report):
    """Cross old/new executables and DLLs, without changing the tested packages."""
    no_game_running()
    assert report['passed'] and not report.get('isolation')
    for mixed, original, dll in (('old-exe-new-sdl', 'baseline', 'candidate'),
                                 ('new-exe-old-sdl', 'candidate', 'baseline')):
        folder = stage / mixed
        folder.mkdir()
        shutil.copytree(stage / original / 'Runtime', folder / 'Runtime')
        shutil.copyfile(stage / dll / 'Runtime/SDL2.dll', folder / 'Runtime/SDL2.dll')
    variants = ('baseline', 'old-exe-new-sdl', 'new-exe-old-sdl', 'candidate')
    results = []
    for repeat, order in enumerate((variants, tuple(reversed(variants)))):
        for variant in order:
            options = SimpleNamespace(exe=stage / variant / 'Runtime/Swordcraft3CustomRendererBeta.exe',
                frames=600, warmup_frames=120, screen='raw', strength=35,
                bios=stage / 'candidate/BIOS/gba_bios.bin',
                rom=ROOT / 'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba',
                state=ROOT / 'validation/translation106-input-audit-1790824852616961300/diagnostic-fixtures/arena-03/combat-ready.gbas')
            result = bench.execute(options, stage / f'isolate-{repeat}-{variant}', 384, bench.MODES[0], stage / 'neutral.trace')
            result.update(variant=variant, repeat=repeat)
            results.append(result)
            print(f'ISOLATE {repeat} {variant}: {result["fps"]:.2f} fps, p99 {result["gap_ms_p99"]:.2f} ms', flush=True)
    assert snapshot(TARGET) == report['target_before']
    report['isolation'] = results
    save(stage, report)


def export_test(stage, report):
    no_game_running()
    assert report['passed'] and not report['installed']
    assert snapshot(TARGET) == report['target_before']
    destination = OWNER / 'release/Portable SDL2 Test'
    assert not destination.exists(), 'Refusing to overwrite an existing test portable'
    for relative, digest in report['candidate'].items():
        assert file_hash(stage / 'candidate' / relative) == digest
    local_copy(destination)
    # Start with untouched player preferences and private copies of saves,
    # never the settings changed by the scripted English/Japanese tests.
    for directory in ('Saves', 'Save States'):
        shutil.copytree(TARGET / directory, destination / directory, dirs_exist_ok=True)
    shutil.copyfile(TARGET / 'README.md', destination / 'README.md')
    for relative in REPLACEMENTS:
        shutil.copyfile(stage / 'candidate' / relative, destination / relative)
    warning = dict(private_only=True, redistributable=False, sdl_version=report['sdl_version'],
        functional_tests_passed=True, performance_approval=False,
        warning='Pacing varied and the rebuilt candidate dipped in widescreen comparisons. Not promoted to the main playable build.',
        source=str(TARGET), validation=str(stage / 'report.json'))
    (destination / 'SDL2-TEST.json').write_text(json.dumps(warning, indent=2) + '\n', encoding='utf-8')
    for relative, digest in report['target_before'].items():
        if relative.startswith(('Settings/', 'Saves/', 'Save States/')):
            assert file_hash(destination / relative) == digest
    assert all(file_hash(destination / p) == h for p, h in report['candidate'].items())
    assert snapshot(TARGET) == report['target_before']
    subprocess.run([str(destination / 'Swordcraft Story 3 Beta.exe'), '--check'],
                   cwd=stage, check=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
    report['test_portable'] = str(destination)
    report['performance_approved'] = False
    save(stage, report)
    print('PRIVATE TEST READY: ' + str(destination), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--recheck', action='store_true')
    parser.add_argument('--isolate', action='store_true')
    parser.add_argument('--export-test', action='store_true')
    args = parser.parse_args()
    if (args.install or args.recheck or args.isolate or args.export_test) and not args.stage:
        parser.error('Installation/recheck/isolation requires a tested --stage')
    if sum((args.install, args.recheck, args.isolate, args.export_test)) > 1:
        parser.error('Choose only one operation')
    directory = args.stage.resolve(strict=True) if args.stage else prepare()
    assert directory.is_relative_to(ROOT / 'validation')
    record = json.loads((directory / 'report.json').read_text())
    if args.install:
        install(directory, record)
    elif args.recheck:
        recheck(directory, record)
    elif args.isolate:
        isolate(directory, record)
    elif args.export_test:
        export_test(directory, record)
    else:
        test(directory, record)
