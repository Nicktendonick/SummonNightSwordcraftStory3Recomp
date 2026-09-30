"""Create a fresh PRIVATE playable Guard test folder, never a public archive.

Stages the allowlisted program/assets, then explicitly copies owned private
test inputs. The normal Portable Beta and every source file remain unchanged.
"""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

from package_portable_release import ROOT, OWNER, BUILD, DATA_DIRS, recipe, run
from package_alpha import file_hash, pe_info, check_dependencies
from benchmark_presentation_filters import DEFAULT_STATE


def main():
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:4]
    target = OWNER / 'release' / ('Guard Experiment '+stamp)
    target.mkdir(parents=True,exist_ok=False)
    for name in DATA_DIRS: (target/name).mkdir()
    sources = {}
    binaries = {}
    strip = Path('C:/msys64/mingw64/bin/strip.exe')
    env = os.environ.copy()
    env['PATH'] = str(strip.parent)+';'+env.get('PATH','')

    def copy(source, relative, binary=False):
        source = source.resolve()
        assert source.is_relative_to(OWNER) and source.is_file()
        dest = target/relative
        assert dest.resolve().is_relative_to(target) and not dest.exists()
        dest.parent.mkdir(parents=True,exist_ok=True)
        sources[str(source)] = file_hash(source)
        shutil.copyfile(source,dest)
        if binary:
            before = pe_info(source)
            run([str(strip),'--strip-debug',str(dest)],env=env)
            after = pe_info(dest)
            for field in ('loaded_sections','imports','entry_rva'):
                assert before[field]==after[field], (relative,field)
            binaries[dest.name]=after

    for source,relative in recipe():
        if relative=='README.md': continue
        copy(source,relative,source.suffix.lower() in ('.exe','.dll'))
    copy(BUILD/'Swordcraft Story 3 Guard Test.exe','Swordcraft Story 3 Guard Test.exe',True)
    check_dependencies(binaries)
    copy(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba','ROMs/Swordcraft Story 3 - English Beta.gba')
    copy(OWNER/'release/swordcraft3_jp - Copy.gba','ROMs/Swordcraft Story 3 - Japanese.gba')
    copy(OWNER/'gbarecomp/bios/gba_bios.bin','BIOS/gba_bios.bin')
    copy(DEFAULT_STATE,'Save States/beta.state2')
    installed = OWNER/'release/Portable Beta'
    # Only save copies; no symlinks/shared save paths or patch cache migration.
    for relative in ('Save States/beta.state1','Saves/battery.eep','Saves/japanese.eep'):
        if (installed/relative).is_file(): copy(installed/relative,relative)
    # Keep the user's input bindings and display preferences, not external paths.
    preferences = installed/'Settings/launcher.ini'
    if preferences.is_file():
        sources[str(preferences)] = file_hash(preferences)
        keep = []
        for line in preferences.read_text(encoding='utf-8-sig').splitlines():
            key = line.split('=',1)[0].strip()
            if key.startswith(('player_key_','player_pad_','assist_')) or key in (
                'scale','fullscreen','linear_filter','sharp_filter','smooth_filter',
                'screen_effect','screen_effect_strength','screen','volume','host_aspect_index'):
                keep.append(line)
        (target/'Settings/launcher.ini').write_text('[Launcher]\n'+'\n'.join(keep)+
            '\nrom_patch_enabled = 0\nskip_launcher = 0\n',encoding='utf-8')
    (target/'Settings/rom.cfg').write_text('../ROMs/Swordcraft Story 3 - English Beta.gba\n')
    (target/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
    (target/'Settings/bios.cfg').write_text('../BIOS/gba_bios.bin\n')
    (target/'README.md').write_text('''# PRIVATE Guard experiment - do not upload this folder

Guard is pre-enabled in this private copy. Current engines use the saved
**Mods > Hold Select to Guard** option; either starter uses that saved choice.
Close any other Swordcraft launcher/game first.

- In normal manual combat, hold your **Select** binding to guard; release it
  to stop (unless normal B is also held). This is not a toggle or auto-block.
- Your selected R ability remains selected. Native jump/attack/action limits
  still apply, and B continues to use your selected ability normally.
- Select no longer turns auto-battle on in this experiment. If an imported
  state already has auto-battle on, Select cancels it before guarding.
- Outside normal combat, Select is unchanged. Scripted/link battles are not
  supported by the experiment and keep their native behavior.
- Keyboard Select is normally Right Shift; controller Select uses its existing
  binding. You can change either under Controls.
- English save-state slot 2 contains a known combat test scene. Slot 1, when
  present, is a copy of your existing slot 1. These are not Japanese states.
- For a comparison without the mod, close the game and open **Swordcraft
  Story 3 Beta.exe** in THIS folder. It explicitly clears the experiment flag.

This folder has independent saves/settings and private ROM/BIOS copies. It is
not a release ZIP. The installed Portable Beta and its saves were not changed.
Both languages build and pass code-layout checks; combat was tested in English.
Japanese has separate save banks and cold-boot/noncombat regression coverage.
Physical controller feel and broader weapons, enemies and damage situations
still need human playtesting. This is not an invulnerability or balance mod.
''',encoding='utf-8')
    check = subprocess.run([str(target/'Swordcraft Story 3 Guard Test.exe'),'--check'],
        cwd=target,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
    assert check.returncode==0
    assert all(file_hash(Path(p))==h for p,h in sources.items()), 'Source input changed'
    report = dict(private_only=True, target=str(target), sources_unchanged=True,
        source_sha256=sources, dependency_closure=True, stripped_loaded_sections_unchanged=True,
        files={p.relative_to(target).as_posix():file_hash(p) for p in target.rglob('*') if p.is_file()})
    (target/'TEST-PACKAGE.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(target,flush=True)


if __name__=='__main__': main()
