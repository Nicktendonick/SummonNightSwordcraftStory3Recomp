"""Promote released English and opt-in framing, with backups and no publication.

Never deletes the old patch, ROM or saves. Only the engines, README, two patch
selection keys and a new named patch are installed. Japanese remains selected
as the patch source. Refuse any unrelated target changes or running game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from package_translation105_test import prepare, smoke, checked, MAIN, PATCH, JP
from update_guard_performance import ROOT, OWNER, stamp, snapshot, replace_with_verified_copy
from promote_portable_beta import no_game_running
from package_alpha import file_hash

def configure_payload(stage):
    stage,record=checked(stage)
    assert json.loads((stage/'SMOKE.json').read_text())['passed']
    # The selected source must really be Japanese, not a retired translated ROM.
    cache=(MAIN/'Settings/rom.cfg').read_text().strip()
    original=Path(cache)
    if not original.is_absolute(): original=MAIN/'Settings'/original
    original=original.resolve(strict=True)
    assert original.is_relative_to(MAIN)
    assert hashlib.sha1(original.read_bytes()).hexdigest()=='3f5253fcf57e07ce52472bd29a61d16b98a12376', 'Select your Japanese ROM before migration'
    dest=stage/'upgrade'; dest.mkdir()
    files=['Runtime/Swordcraft3CustomRendererBeta.exe','Runtime/Swordcraft3Japanese.exe','README.md']
    for relative in files:
        p=dest/relative; p.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(stage/'payload'/relative,p)
    patch_relative='Mods/'+PATCH.name
    (dest/'Mods').mkdir()
    shutil.copyfile(PATCH,dest/patch_relative)
    assert file_hash(PATCH)==record['source_hashes'][str(PATCH)]
    config=MAIN/'Settings/launcher.ini'
    lines=config.read_text(encoding='utf-8').splitlines(keepends=True)
    replacements={'rom_patch_enabled':'1','rom_patch_path':'../'+patch_relative}
    section=False; seen=set()
    for i,line in enumerate(lines):
        text=line.strip()
        if text.startswith('['): section=text=='[Launcher]'
        if section and '=' in text:
            key=text.split('=',1)[0].strip()
            if key in replacements:
                assert key not in seen
                seen.add(key); lines[i]=key+' = '+replacements[key]+'\n'
    assert seen==set(replacements)
    (dest/'Settings').mkdir()
    (dest/'Settings/launcher.ini').write_text(''.join(lines),encoding='utf-8')
    # This is the exact root/settings/patch layout to be installed. Test through
    # the real launcher, not a direct ROM-hash override.
    from benchmark_presentation_filters import environment
    from update_graphics_presets import execute
    test=stage/'test'
    shutil.copyfile(dest/'Settings/launcher.ini',test/'Settings/launcher.ini')
    relative_original=original.relative_to(MAIN)
    target=test/relative_original; target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(original,target)
    shutil.copyfile(MAIN/'Settings/rom.cfg',test/'Settings/rom.cfg')
    shutil.copyfile(dest/patch_relative,test/patch_relative)
    env=environment(test,384,stage/'neutral.trace',True)
    env.pop('SDL_VIDEODRIVER',None); env.pop('SDL_RENDER_DRIVER',None)
    env['LNG_SCRIPT']='wait:8;romprobe;view:mods;builtinprobe;wait:3;quit'
    log=execute(test/'Runtime/Swordcraft3CustomRendererBeta.exe',
        ['--window','--launcher',str(test/'Runtime/game.toml')],stage,env,'migration')
    assert 'verified=1 can_play=1 patch=1 language=English (translation 1.0.5.f)' in log
    assert snapshot(MAIN)==record['main_before']
    update=dict(files=snapshot(dest),source_rom_sha256=file_hash(original),migration_ui_verified=True)
    (stage/'UPGRADE.json').write_text(json.dumps(update,indent=2)+'\n')
    print('UPGRADE='+str(stage),flush=True)

def install(stage):
    stage,record=checked(stage)
    update=json.loads((stage/'UPGRADE.json').read_text())
    assert update['migration_ui_verified'] and snapshot(stage/'upgrade')==update['files']
    assert json.loads((stage/'SMOKE.json').read_text())['passed']
    no_game_running()
    assert 'Runtime/package-manifest.json' not in record['main_before'], 'Needs manifest-aware update'
    assert all(file_hash(Path(p))==h for p,h in record['source_hashes'].items())
    backup=OWNER/'release'/('Portable Beta Before English 1.0.5f '+stamp()); backup.mkdir()
    for relative in update['files']:
        if relative in record['main_before']:
            p=backup/relative; p.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(MAIN/relative,p)
            assert file_hash(p)==record['main_before'][relative]
    expected=dict(record['main_before']); expected.update(update['files'])
    changed=[]
    try:
        no_game_running(); assert snapshot(MAIN)==record['main_before']
        for relative in update['files']:
            target=MAIN/relative
            assert target.resolve().is_relative_to(MAIN.resolve())
            target.parent.mkdir(parents=True,exist_ok=True)
            replace_with_verified_copy(stage/'upgrade'/relative,target); changed.append(relative)
        subprocess.run([str(MAIN/'Swordcraft Story 3 Beta.exe'),'--check'],check=True,
            cwd=stage,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        assert snapshot(MAIN)==expected
    except BaseException:
        for relative in reversed(changed):
            if relative in record['main_before']:
                replace_with_verified_copy(backup/relative,MAIN/relative)
            else:
                # Only an exact new file this updater installed may be removed.
                target=MAIN/relative
                assert target.resolve().is_relative_to(MAIN.resolve()) and file_hash(target)==update['files'][relative]
                target.unlink()
        raise
    result=dict(installed=True,target=str(MAIN),backup=str(backup),files=update['files'],
                existing_saves_and_other_files_unchanged=True,old_patch_preserved=True)
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(result,indent=2)+'\n')
    (stage/'INSTALLED.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['prepare','smoke','install'])
    p.add_argument('--stage',type=Path)
    a=p.parse_args()
    if a.action=='prepare': prepare(release_readme=True)
    elif a.action=='smoke':
        assert a.stage
        smoke(a.stage); configure_payload(a.stage)
    else:
        assert a.stage
        install(a.stage)
