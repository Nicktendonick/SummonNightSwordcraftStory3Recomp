"""Test and install only border/shadow engines + README, with verified rollback.

Reuses the released-English private package smoke and state-input battle cases.
Never changes main settings, saves, ROMs, BIOS or patch files; no publication.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

from package_translation105_test import prepare, smoke, checked, MAIN
from update_guard_performance import OWNER, snapshot, replace_with_verified_copy, stamp
from promote_portable_beta import no_game_running
from package_alpha import file_hash
from validate_battle_camera import exercise

FILES=('Runtime/Swordcraft3CustomRendererBeta.exe','Runtime/Swordcraft3Japanese.exe','README.md')

def test(stage):
    no_game_running()
    stage,record=checked(stage)
    # The UI smoke changes only its own copied settings. A retry of battle
    # assertions may reuse successful smoke for the same immutable payload.
    smoke_record=stage/'SMOKE.json'
    if not smoke_record.exists() or not json.loads(smoke_record.read_text())['passed']:
        smoke(stage)
    assert all(file_hash(Path(p))==h for p,h in record['source_hashes'].items())
    old=MAIN/FILES[0]
    candidate=stage/'payload'/FILES[0]
    run=stage/('border-check-'+stamp()); run.mkdir()
    result=dict(passed=False,cases=[],run=str(run),payload={p:record['payload'][p] for p in FILES})
    cases=[(a,384,True) for a in (0,2,3,7,1)]+[(0,w,True) for w in (240,284)]+[(0,384,False)]
    try:
        for arena,width,enabled in cases:
            baseline,_=exercise(arena,width,enabled,run/f'old-{arena}-{width}-{int(enabled)}',old)
            out=run/f'new-{arena}-{width}-{int(enabled)}'
            current,summary=exercise(arena,width,enabled,out,candidate)
            # Every recorded guest-state hash must match, not just player XY.
            assert current==baseline,(arena,width,enabled,'guest state changed')
            log=(out/'stderr.log').read_text(errors='replace')
            rows=[{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
                  for line in re.findall(r'\[sc3:composition\] ([^\r\n]+)',log)]
            if width==240:
                # Native scanout bypasses the complete host compositor.
                native=re.findall(r'\[sc3:state-frame\] ([^\r\n]+)',log)
                assert len(native)>=summary['frames'] and all('wide=0 ' in r for r in native)
                assert not rows
            else:
                assert rows and all('edge_columns' in r and 'edge_rows' in r for r in rows)
            decorated=[r for r in rows if r['edge_columns']]
            expected=enabled and width==384 and arena in (0,2,3,7)
            assert bool(decorated)==expected,(arena,width,enabled,'edge dispatch')
            for r in decorated:
                assert r['complete_owner'] and 0<r['edge_rows']<=125
                assert r['edge_columns']==16*r['edge_rows']
            result['cases'].append(dict(summary,guest_state_identical=True,
                decorated_frames=len(decorated),no_framebuffer_assertions=True))
        assert snapshot(MAIN)==record['main_before']
        assert all(file_hash(Path(p))==h for p,h in record['source_hashes'].items())
        result['passed']=True
    finally:
        (stage/'BORDER.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: exact guest-state equality, active edge dispatch, both limits, off/native/16:9/unsupported fallback',flush=True)

def install(stage):
    stage,record=checked(stage)
    tested=json.loads((stage/'BORDER.json').read_text())
    payload={p:record['payload'][p] for p in FILES}
    assert tested['passed'] and tested['payload']==payload
    assert json.loads((stage/'SMOKE.json').read_text())['passed']
    assert all(file_hash(Path(p))==h for p,h in record['source_hashes'].items())
    assert all(p in record['main_before'] for p in FILES)
    assert 'Runtime/package-manifest.json' not in record['main_before']
    assert 'Runtime/SHA256SUMS.txt' not in record['main_before']
    no_game_running()
    backup=OWNER/'release'/('Portable Beta Before Border Shadow UTC '+stamp())
    backup.mkdir()
    for relative in FILES:
        target=backup/relative; target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(MAIN/relative,target)
        assert file_hash(target)==record['main_before'][relative]
    expected=dict(record['main_before']); expected.update(payload)
    changed=[]
    try:
        no_game_running(); assert snapshot(MAIN)==record['main_before']
        for relative in FILES:
            target=MAIN/relative
            assert target.resolve(strict=True).is_relative_to(MAIN.resolve(strict=True))
            replace_with_verified_copy(stage/'payload'/relative,target); changed.append(relative)
        subprocess.run([str(MAIN/'Swordcraft Story 3 Beta.exe'),'--check'],check=True,
            cwd=stage,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        assert snapshot(MAIN)==expected
    except BaseException:
        for relative in reversed(changed):
            replace_with_verified_copy(backup/relative,MAIN/relative)
        raise
    result=dict(installed=True,target=str(MAIN),backup=str(backup),files=payload,
                all_settings_saves_and_other_files_unchanged=True)
    (stage/'INSTALLED.json').write_text(json.dumps(result,indent=2)+'\n')
    (backup/'UPDATE-REPORT.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['prepare','test','install'])
    parser.add_argument('--stage',type=Path)
    args=parser.parse_args()
    if args.action=='prepare':
        no_game_running(); prepare(release_readme=True)
    else:
        assert args.stage, '--stage required'
        (test if args.action=='test' else install)(args.stage)
