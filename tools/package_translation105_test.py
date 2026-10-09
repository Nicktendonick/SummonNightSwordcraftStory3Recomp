"""Stage an isolated private translation test; never update Portable Beta.

No ZIP/publication. Clean allowlist, fresh saves, matching dual engines, local
ROM/BIOS copies, PE dependency audit and protected main-install hashes.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from package_portable_release import recipe, DATA_DIRS, STARTER, GAME, JAPANESE
from package_alpha import file_hash, pe_info, check_dependencies
from update_guard_performance import snapshot
from validate_translation105 import ROOT, OWNER, ROM, JP, audit

MAIN = OWNER/'release/Portable Beta'
PATCH = OWNER.parent/'Swordcraft Story 3 rom files/Hajimari_no_Ishi_v1.0.5.f.bps'
BIOS = OWNER/'gbarecomp/bios/gba_bios.bin'
STRIP = Path('C:/msys64/mingw64/bin/strip.exe')


def prepare(release_readme=False):
    stage = ROOT/'validation'/('release105-'+str(time.time_ns()))
    payload, test = stage/'payload', stage/'test'
    payload.mkdir(parents=True)
    before = snapshot(MAIN)
    audit(stage)
    binaries, sources = {}, {}
    for src, relative in recipe():
        if relative == 'Runtime/'+GAME:
            src = ROOT/'build-native/Swordcraft3Translation105.exe'
        elif relative == 'Runtime/'+JAPANESE:
            src = ROOT/'build-native/Swordcraft3Japanese105.exe'
        elif relative == 'README.md':
            src = ROOT/('packaging/portable-beta/README.md' if release_readme else 'packaging/translation105-test/README.md')
        assert src.is_file() and not src.is_symlink(), src
        sources[str(src)] = file_hash(src)
        target = payload/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
        if target.suffix.lower() in ('.exe', '.dll'):
            original = pe_info(src)
            subprocess.run([str(STRIP), '--strip-debug', str(target)], check=True, timeout=60,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            candidate = pe_info(target)
            assert all(candidate[k] == original[k] for k in ('loaded_sections','imports','entry_rva'))
            binaries[target.name] = candidate
        assert sources[str(src)] == file_hash(src)
    check_dependencies(binaries)
    for name in DATA_DIRS:
        (payload/name).mkdir(exist_ok=True)
    # This is deliberately a PRIVATE runnable test, not a distribution archive.
    for src, relative in ((ROM,'ROMs/Swordcraft Story 3 English 1.0.5f.gba'),
                          (JP,'ROMs/Swordcraft Story 3 Japanese.gba'),
                          (BIOS,'BIOS/gba_bios.bin'),(PATCH,'Mods/'+PATCH.name)):
        sources[str(src)] = file_hash(src)
        shutil.copyfile(src,payload/relative)
        assert file_hash(payload/relative) == sources[str(src)]
    (payload/'Settings/rom.cfg').write_text('../ROMs/Swordcraft Story 3 English 1.0.5f.gba\n')
    (payload/'Settings/bios.cfg').write_text('../BIOS/gba_bios.bin\n')
    assert not list((payload/'Saves').iterdir()) and not list((payload/'Save States').iterdir())
    shutil.copytree(payload,test)
    assert snapshot(MAIN) == before
    record = dict(main_before=before, source_hashes=sources, payload=snapshot(payload),
                  private=True, publishable=False, dependency_closure=True,
                  fresh_saves=True, strip_preserves_loaded_sections=True)
    (stage/'PREPARED.json').write_text(json.dumps(record,indent=2)+'\n')
    print('STAGE='+str(stage),flush=True)
    return stage


def checked(stage):
    stage = stage.resolve(strict=True)
    assert stage.parent == ROOT/'validation' and stage.name.startswith('release105-')
    record = json.loads((stage/'PREPARED.json').read_text())
    assert snapshot(stage/'payload') == record['payload']
    assert snapshot(MAIN) == record['main_before'], 'Main install changed externally'
    return stage, record


def smoke(stage):
    from update_graphics_presets import execute
    from benchmark_presentation_filters import environment
    stage, record = checked(stage)
    package = stage/'test'
    system = os.environ.get('SystemRoot',os.environ.get('SYSTEMROOT'))
    neutral = stage/'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n')
    result = dict(passed=False)
    try:
        env = environment(package,384,neutral,True)
        # The launcher uses OpenGL, which SDL's dummy driver cannot provide.
        env.pop('SDL_VIDEODRIVER',None)
        env.pop('SDL_RENDER_DRIVER',None)
        env['PATH'] = system+'/System32;'+system
        subprocess.run([str(package/STARTER),'--check'],check=True,env=env,timeout=20,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        # Import through the real UI model. No direct state/ROM-gate bypass.
        env['LNG_SCRIPT'] = ('wait:8;romprobe;romfile:'+str(JP)+';romprobe;patchfile:'+str(PATCH)+
            ';romprobe;patchtoggle;romprobe;patchtoggle;romprobe;'
            'view:mods;builtinprobe;builtintoggle:0;builtinprobe;'
            'builtintoggle:1;builtinprobe;'
            'view:settings;preset:1;presetprobe;screenprobe;aspectprobe;wait:3;quit')
        exe = package/'Runtime'/GAME
        config = package/'Runtime/game.toml'
        log = execute(exe,['--window','--launcher',str(config)],stage,env,'import')
        assert 'verified=1 can_play=1 patch=0 language=Japanese (original)' in log
        assert 'verified=1 can_play=1 patch=1 language=English (translation 1.0.5.f)' in log
        assert 'id=select-guard enabled=1' in log
        assert 'id=bounded-battle-camera enabled=1' in log
        result['ui_patch_import_toggle'] = True
        env['LNG_SCRIPT']='wait:8;view:mods;builtinprobe;builtintoggle:1;builtinprobe;wait:3;quit'
        log=execute(exe,['--window','--launcher',str(config)],stage,env,'camera-reopen')
        assert 'id=bounded-battle-camera enabled=1' in log and 'id=bounded-battle-camera enabled=0' in log
        result['camera_launcher_persistence']=True
        env.pop('LNG_SCRIPT')
        # Cold-boot both engines; test handoff from the opposite entry engine.
        result['languages'] = []
        for language, entry, rom in [('english',JAPANESE,ROM),('japanese',GAME,JP)]:
            env['GBARECOMP_ASSIST_SCRIPT'] = ('120:menu_open;125:menu_probe;130:menu_preset=2;'
                '135:menu_screen=3;140:menu_resume;550:menu_open;555:menu_reset;560:menu_confirm')
            env['GBARECOMP_ASSIST_SCRIPT_AFTER_RESET'] = '60:menu_open;65:menu_probe;70:menu_close;75:menu_confirm'
            args = ['--window','--no-launcher','--frames','650','--rom',str(rom),'--bios',str(BIOS),
                    '--save',str(package/'Saves'/('smoke-'+language+'.eep')),str(config)]
            log = execute(package/'Runtime'/entry,args,stage,env,language)
            stdout = (stage/(language+'.out')).read_text(errors='replace')
            assert 'dispatch_misses=0' in stdout, stdout[-3000:]
            assert 'Clean reset:' in log
            assert '[runtime-preset] index=2 name=Soft & Smooth' in log
            assert 'authenticated=1' in log
            result['languages'].append(dict(language=language,cold_boot=True,reset=True,
                sibling_handoff=True,guard_authenticated=True,no_dispatch_misses=True))
        assert snapshot(MAIN) == record['main_before']
        assert all(file_hash(Path(p)) == h for p,h in record['source_hashes'].items())
        result.update(passed=True,main_untouched=True,inputs_unchanged=True)
    finally:
        (stage/'SMOKE.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: isolated patch import, Japanese/English handoff, boot, filters, reset; main untouched',flush=True)


def install(stage):
    stage, record = checked(stage)
    assert json.loads((stage/'SMOKE.json').read_text())['passed']
    target = OWNER/'release'/('Translation 1.0.5f Test '+time.strftime('%Y%m%d-%H%M%S'))
    assert not target.exists() and target.parent.resolve() == (OWNER/'release').resolve()
    shutil.copytree(stage/'payload',target)
    assert snapshot(target) == record['payload']
    assert snapshot(MAIN) == record['main_before']
    (stage/'INSTALLED.json').write_text(json.dumps(dict(path=str(target),main_untouched=True),indent=2)+'\n')
    print('TEST_BUILD='+str(target),flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['prepare','smoke','install'])
    p.add_argument('--stage',type=Path)
    a = p.parse_args()
    if a.action == 'prepare': prepare()
    else:
        assert a.stage, '--stage required'
        (smoke if a.action == 'smoke' else install)(a.stage)
