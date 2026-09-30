"""Stage or install ONLY the two engines in an existing PRIVATE Guard test.

Preparation strips debug data and proves loaded PE sections are unchanged.
Install refuses a running/changed target, retains verified rollback copies,
and checks that every other file (including settings and saves) is unchanged.
Never creates an archive or changes the normal Portable Beta.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

from package_alpha import file_hash, pe_info, check_dependencies

ROOT = Path(__file__).resolve().parents[1]
OWNER = ROOT.parents[1]
ENGINES = ('Swordcraft3CustomRendererBeta.exe', 'Swordcraft3Japanese.exe')
DLLS = ('SDL2.dll', 'libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll')


def stamp():
    return datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:4]


def check_target(target):
    target = target.resolve(strict=True)
    if target.parent != OWNER/'release' or not target.name.startswith('Guard Experiment '):
        raise ValueError('Target must be an existing private Guard Experiment under owner/release')
    if not json.loads((target/'TEST-PACKAGE.json').read_text())['private_only']:
        raise ValueError('Not a private test package')
    # Query process paths, never terminate a user process.
    command = "Get-CimInstance Win32_Process | Where-Object Name -Like '*Swordcraft*' | Select-Object ProcessId,ExecutablePath | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell', '-NoProfile', '-Command', command],
        capture_output=True, text=True, check=True, timeout=20,
        creationflags=subprocess.CREATE_NO_WINDOW)
    rows = json.loads(result.stdout) if result.stdout.strip() else []
    if isinstance(rows, dict):
        rows = [rows]
    for row in rows:
        path = row.get('ExecutablePath')
        if not path or Path(path).resolve().is_relative_to(target):
            raise RuntimeError(f'Close the running Swordcraft process first: {row["ProcessId"]}')
    return target


def snapshot(target):
    result = {}
    for path in target.rglob('*'):
        if not path.resolve().is_relative_to(target):
            raise ValueError('Target contains an external link')
        if path.is_file():
            result[path.relative_to(target).as_posix()] = file_hash(path)
    return result


def prepare(target):
    target = check_target(target)
    before = snapshot(target)
    stage = ROOT/'validation'/('smooth-engine-candidate-'+stamp())
    stage.mkdir(parents=True, exist_ok=False)
    binaries = {}
    sources = {}
    for name in (*ENGINES, *DLLS):
        source = (ROOT/'build-native'/name) if name in ENGINES else target/'Runtime'/name
        destination = stage/name
        sources[str(source)] = file_hash(source)
        shutil.copyfile(source, destination)
        original = pe_info(source)
        if name in ENGINES:
            subprocess.run(['C:/msys64/mingw64/bin/strip.exe', '--strip-debug', str(destination)],
                check=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        candidate = pe_info(destination)
        for field in ('loaded_sections', 'imports', 'entry_rva'):
            if original[field] != candidate[field]:
                raise RuntimeError(f'Stripping changed {name}: {field}')
        binaries[name] = candidate
    check_dependencies(binaries)
    if snapshot(target) != before:
        raise RuntimeError('Target changed during preparation')
    report = dict(target=str(target), before=before, source_sha256=sources,
        candidate_sha256={name:file_hash(stage/name) for name in (*ENGINES,*DLLS)},
        loaded_sections_unchanged=True, dependency_closure=True)
    (stage/'PREPARED.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(stage, flush=True)


def replace_with_verified_copy(source, destination):
    temporary = destination.with_name(destination.name+'.update-'+uuid.uuid4().hex)
    shutil.copyfile(source, temporary)
    if file_hash(temporary) != file_hash(source):
        raise RuntimeError('Copy verification failed; temporary retained')
    os.replace(temporary, destination)


def install(stage):
    stage = stage.resolve(strict=True)
    if stage.parent != ROOT/'validation' or not stage.name.startswith('smooth-engine-candidate-'):
        raise ValueError('Install needs a prepared candidate from this project')
    report = json.loads((stage/'PREPARED.json').read_text())
    target = check_target(Path(report['target']))
    if snapshot(target) != report['before']:
        raise RuntimeError('Target changed since preparation; prepare a fresh candidate')
    for name, expected in report['candidate_sha256'].items():
        if name not in (*ENGINES, *DLLS) or file_hash(stage/name) != expected:
            raise RuntimeError('Candidate changed since preparation')
    backup = OWNER/'release'/('Guard Performance Rollback '+stamp())
    backup.mkdir(exist_ok=False)
    for name in ENGINES:
        shutil.copyfile(target/'Runtime'/name, backup/name)
        if file_hash(backup/name) != report['before']['Runtime/'+name]:
            raise RuntimeError('Rollback copy verification failed')
    replaced = []
    try:
        for name in ENGINES:
            replace_with_verified_copy(stage/name, target/'Runtime'/name)
            replaced.append(name)
        subprocess.run([str(target/'Swordcraft Story 3 Guard Test.exe'), '--check'],
            cwd=stage, check=True, timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
        after = snapshot(target)
        expected = dict(report['before'])
        for name in ENGINES:
            expected['Runtime/'+name] = report['candidate_sha256'][name]
        if after != expected:
            raise RuntimeError('Unexpected target file changes')
    except BaseException:
        for name in replaced:
            replace_with_verified_copy(backup/name, target/'Runtime'/name)
        raise
    report.update(installed=True, backup=str(backup), all_other_files_unchanged=True,
        installed_sha256={name:after['Runtime/'+name] for name in ENGINES})
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({key:report[key] for key in ('target','backup','installed','all_other_files_unchanged','installed_sha256')}, indent=2), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--prepare', type=Path, metavar='PRIVATE_GUARD_FOLDER')
    group.add_argument('--install', type=Path, metavar='PREPARED_CANDIDATE')
    args = parser.parse_args()
    prepare(args.prepare) if args.prepare else install(args.install)


if __name__ == '__main__':
    main()
