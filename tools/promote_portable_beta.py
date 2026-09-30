"""Promote the tested private engines/starters to the owner's main portable install.

Explicit program/notices allowlist, whole-folder change detection, verified rollback.
Never copies private game inputs, creates a release ZIP, or publishes anything.
Prepare first; edit only the staged README if needed; then install and verify.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from package_alpha import file_hash, pe_info, check_dependencies
from package_portable_release import recipe
from update_guard_performance import ROOT, OWNER, stamp, snapshot, replace_with_verified_copy

TARGET = OWNER/'release/Portable Beta'
SOURCE = OWNER/'release/Guard Experiment 20260929-023325-a2fa'
PINS = {
    'Runtime/Swordcraft3CustomRendererBeta.exe': '324057340aaa3667cb22118b65a63f720c1ba318d72c57a61417af0a12d1f4ee',
    'Runtime/Swordcraft3Japanese.exe': 'c72c0c7afd6c2e4a56760ae918a9f21ffce6df055c93a67362dc587ab27148bf',
    'Swordcraft Story 3 Beta.exe': '7a5f6a8f82d189b3e7a3691867781e2414db3d208298f97820aa51047839596b',
    'Swordcraft Story 3 Guard Test.exe': '5cf893ee4eced26a88ba4a21ad48a2e73277bcabbafc6fa5a41b84e8cc33e645',
    'Runtime/libgcc_s_seh-1.dll': 'b5d60ef97f1ee7852fe2e61c531a0c5a0b80a7d0c19ba08f975e46144daa1c29',
    'Runtime/libstdc++-6.dll': '388c134faffd39bc9e6562af0aebc18aa0054742670265860062532ea369a384',
    'Runtime/libwinpthread-1.dll': 'ff64d4e0933c3f1e9f3a19c49fce3f4083a144829654f69dc3b993221c5db57f',
    'Runtime/SDL2.dll': '23e157d746014c31411f67a5477b12b29072ab42abca6c78e7babb4dfbfbc3ac',
}
NOTICES = {relative: source for source, relative in recipe() if relative.startswith('Runtime/notices/')}
ALLOWLIST = set(PINS) | set(NOTICES) | {'README.md'}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def no_game_running():
    command = "$ErrorActionPreference='Stop'; Get-CimInstance Win32_Process | Where-Object {$_.Name -match 'Swordcraft|SummonNight'} | Select-Object ProcessId,Name | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell', '-NoProfile', '-Command', command],
        capture_output=True, text=True, check=True, timeout=20,
        creationflags=subprocess.CREATE_NO_WINDOW)
    require(not result.stdout.strip(), 'Close the game/launcher first: '+result.stdout)


def location_checks():
    require(TARGET.resolve(strict=True) == TARGET, 'Unexpected target link/location')
    require(SOURCE.resolve(strict=True) == SOURCE, 'Unexpected source link/location')
    no_game_running()


def provenance():
    records = {}
    git = 'C:/Program Files/Git/cmd/git.exe'
    for name, directory in [('game', ROOT), ('runtime', ROOT/'gbarecomp'), ('ui', ROOT/'recomp-ui')]:
        def run(*args):
            return subprocess.run([git, '-C', str(directory), *args], check=True,
                stdout=subprocess.PIPE, timeout=20).stdout
        # Record untracked contents too; HEAD/diff alone omits new implementation.
        changed = set(run('ls-files', '-m', '-o', '--exclude-standard', '-z').decode().split('\0'))-{''}
        hashes = {}
        for relative in sorted(changed):
            path = directory/relative
            if path.is_file():
                require(path.resolve().is_relative_to(OWNER), 'External source link')
                hashes[relative] = file_hash(path)
        records[name] = dict(head=run('rev-parse','HEAD').decode().strip(),
            status=run('status','--short').decode().splitlines(), changed_file_sha256=hashes)
    return records


def prepare():
    location_checks()
    before, source_before = snapshot(TARGET), snapshot(SOURCE)
    for relative, digest in PINS.items():
        require(source_before.get(relative)==digest, 'Tested source changed: '+relative)
    require('Swordcraft Story 3 Guard Test.exe' not in before, 'Guard starter already exists; review before replacing')
    require('Runtime/package-manifest.json' not in before and 'Runtime/SHA256SUMS.txt' not in before,
            'Manifest-bearing install needs a manifest-aware update')
    for relative, digest in source_before.items():
        if relative.startswith('Runtime/assets/') or relative == 'Runtime/game.toml':
            require(before.get(relative)==digest, 'Runtime dependency/asset differs: '+relative)
    for relative, source in NOTICES.items():
        require(source_before.get(relative)==file_hash(source), 'Notice differs from packaging source: '+relative)
    binaries = {}
    for relative in PINS:
        original = pe_info(SOURCE/relative)
        current = pe_info(ROOT/'build-native'/Path(relative).name)
        for field in ('loaded_sections','imports','entry_rva'):
            require(original[field]==current[field], 'Tested binary differs from current build: '+relative+' '+field)
        binaries[Path(relative).name] = original
    check_dependencies(binaries)
    stage = ROOT/'validation'/('portable-promotion-'+stamp())
    stage.mkdir()
    payload = stage/'payload'
    for relative in sorted(ALLOWLIST):
        destination = payload/relative
        destination.parent.mkdir(parents=True,exist_ok=True)
        source = TARGET/relative if relative=='README.md' else SOURCE/relative
        shutil.copyfile(source,destination)
        require(file_hash(source)==file_hash(destination), 'Staging mismatch')
    require(snapshot(TARGET)==before and snapshot(SOURCE)==source_before, 'Installation changed during preparation')
    report = dict(target=str(TARGET), source=str(SOURCE), before=before,
        source_before=source_before, pins=PINS, current_source=provenance(),
        loaded_sections_match_current_build=True, dependency_closure=True)
    (stage/'PREPARED.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(stage,flush=True)


def checked_stage(stage):
    stage = stage.resolve(strict=True)
    require(stage.parent==ROOT/'validation' and stage.name.startswith('portable-promotion-'), 'Invalid stage')
    report = json.loads((stage/'PREPARED.json').read_text(encoding='utf-8'))
    require(report['target']==str(TARGET) and report['source']==str(SOURCE) and report['pins']==PINS, 'Stage identity mismatch')
    return stage, report


def install(stage):
    stage, report = checked_stage(stage)
    location_checks()
    require(snapshot(TARGET)==report['before'], 'Main portable changed; prepare again')
    require(snapshot(SOURCE)==report['source_before'], 'Test package changed; prepare again')
    payload = stage/'payload'
    staged = snapshot(payload)
    require(set(staged)==ALLOWLIST, 'Unexpected staged payload')
    require(all(staged[p]==h for p,h in PINS.items()), 'Pinned binary changed')
    require(all(staged[p]==report['source_before'][p] for p in NOTICES), 'Notice changed')
    require(0 < (payload/'README.md').stat().st_size < 65536, 'Invalid staged README')
    backup = OWNER/'release'/('Portable Beta Rollback '+stamp())
    backup.mkdir()
    for relative in sorted(ALLOWLIST & report['before'].keys()):
        dest = backup/relative
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(TARGET/relative,dest)
        require(file_hash(dest)==report['before'][relative], 'Rollback copy mismatch')
    expected = dict(report['before'])
    expected.update(staged)
    update = dict(report, installed=False, backup=str(backup), after_expected=expected,
        updated_files=staged, added_files=sorted(ALLOWLIST-report['before'].keys()),
        untouched_file_count=len(set(report['before'])-ALLOWLIST))
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(update,indent=2),encoding='utf-8')
    replaced = []
    try:
        # Recheck after backup work, immediately before touching the installation.
        no_game_running()
        require(snapshot(TARGET)==report['before'], 'Target changed during backup')
        for relative in sorted(ALLOWLIST):
            (TARGET/relative).parent.mkdir(parents=True,exist_ok=True)
            replace_with_verified_copy(payload/relative,TARGET/relative)
            replaced.append(relative)
        for name in ('Swordcraft Story 3 Beta.exe','Swordcraft Story 3 Guard Test.exe'):
            subprocess.run([str(TARGET/name),'--check'],cwd=stage,check=True,timeout=15,
                creationflags=subprocess.CREATE_NO_WINDOW)
        require(snapshot(TARGET)==expected, 'Unexpected installation changes')
        require(snapshot(SOURCE)==report['source_before'], 'Test source changed')
    except BaseException:
        for relative in reversed(replaced):
            if relative in report['before']:
                replace_with_verified_copy(backup/relative,TARGET/relative)
            else:
                recovered = backup/'recovered-added-files'/relative
                recovered.parent.mkdir(parents=True,exist_ok=True)
                shutil.move(str(TARGET/relative),str(recovered))
        raise
    update.update(installed=True, all_other_files_unchanged=True, both_preflights_passed=True)
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(update,indent=2),encoding='utf-8')
    (stage/'INSTALLED.json').write_text(json.dumps(update,indent=2),encoding='utf-8')
    print(json.dumps({k:update[k] for k in ('target','backup','installed','updated_files','added_files','untouched_file_count','all_other_files_unchanged')},indent=2),flush=True)


def verify(stage):
    stage, _ = checked_stage(stage)
    report = json.loads((stage/'INSTALLED.json').read_text(encoding='utf-8'))
    require(report['installed'] and snapshot(TARGET)==report['after_expected'], 'Installed files differ from promotion record')
    require(snapshot(SOURCE)==report['source_before'], 'Private source folder changed')
    print('PASS: exact installed payload; all other portable and private-source files unchanged',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    action=parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare',action='store_true')
    action.add_argument('--install',type=Path)
    action.add_argument('--verify',type=Path)
    args=parser.parse_args()
    if args.prepare: prepare()
    elif args.install: install(args.install)
    else: verify(args.verify)
