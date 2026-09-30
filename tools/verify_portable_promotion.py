"""Isolated post-promotion launcher and live-menu smoke checks (no pixels)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

import playtest_smooth_guard as play
import promote_portable_beta as promote


def main(stage):
    promote.verify(stage)
    promote.no_game_running()
    out = promote.ROOT/'validation'/('prsm-'+uuid.uuid4().hex[:8])
    out.mkdir()
    target = promote.TARGET
    fresh = out/'pkg'
    fresh.mkdir()
    # Copy only program/assets/config/credits. No private ROM/BIOS/settings/saves.
    relatives = list(promote.PINS) + ['Runtime/game.toml']
    relatives += [p.relative_to(target).as_posix() for directory in ('Runtime/assets','Credits')
                  for p in (target/directory).rglob('*') if p.is_file()]
    for relative in relatives:
        destination = fresh/relative
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(target/relative,destination)
        promote.require(promote.file_hash(destination)==promote.file_hash(target/relative), 'Smoke copy mismatch')
    payload = promote.snapshot(fresh)
    report = dict(installation=str(target), isolated_copy=str(fresh), passed=False, launchers={})
    print('Smoke evidence: '+str(out),flush=True)
    try:
        for name in ('Swordcraft Story 3 Beta.exe','Swordcraft Story 3 Guard Test.exe'):
            env = {k:v for k,v in os.environ.items() if not k.startswith(('GBARECOMP_','SWORDCRAFT3_','SDL_','LNG_'))}
            system_root = env.get('SYSTEMROOT') or env.get('SystemRoot')
            promote.require(bool(system_root), 'Windows system directory unavailable')
            env['PATH'] = os.path.join(system_root,'System32')+';'+system_root
            # The actual starters keep LNG_SCRIPT but clear gameplay override vars.
            env['LNG_SCRIPT'] = ('wait:10;romprobe;view:settings;scaling:smooth;effect:lcd;effectstrength:35;'
                'filterprobe;aspectcycle;aspectprobe;aspectcycle;aspectprobe;aspectcycle;aspectprobe;'
                'view:controller;wait:5;view:assist_tools;wait:5;view:mods;wait:5;view:credits;wait:5;quit')
            before_logs = set((fresh/'Captures').glob('*/session.log'))
            child = subprocess.Popen([str(fresh/name)],cwd=system_root,env=env,
                creationflags=subprocess.CREATE_NO_WINDOW)
            # On timeout retain the owned test for diagnosis; never kill a user's game.
            child.wait(timeout=30)
            promote.require(child.returncode==0, 'Launcher failed: '+name)
            logs = set((fresh/'Captures').glob('*/session.log'))-before_logs
            promote.require(len(logs)==1, 'Missing launcher log: '+name)
            log = next(iter(logs)).read_text(errors='replace')
            promote.require('[dbg] script:' in log and '[launcher:probe] verified=0 can_play=0' in log,
                            'Empty-input launcher verification missing: '+name)
            promote.require('smooth=1 effect=1 strength=35' in log, 'Filter model not updated')
            for index in range(3):
                promote.require('[launcher:aspect] index='+str(index) in log, 'Missing aspect model')
            for folder in ('ROMs','BIOS','Saves','Save States'):
                promote.require(not any(p.is_file() for p in (fresh/folder).rglob('*')), 'Private input borrowed: '+folder)
            promote.require(all(promote.file_hash(fresh/p)==h for p,h in payload.items()), 'Fresh launcher changed original payload')
            report['launchers'][name] = dict(clean_exit=True, empty_inputs=True,
                reduced_path=True, filter_model=True, aspects=[0,1,2], original_payload_unchanged=True)
            print('PASS: fresh launcher '+name,flush=True)
        # Exercise exact installed engine with an explicit private data root.
        play.EXE = target/'Runtime/Swordcraft3CustomRendererBeta.exe'
        trace = out/'neutral.trace'
        trace.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n',encoding='ascii')
        report['menu'] = play.menu_playtest(out/'menu',play.bench.DEFAULT_STATE,trace)
        promote.verify(stage)
        report.update(passed=True, player_files_unchanged=True)
        print('PASS: installed-engine pause/resume/aspects/filter/state menu',flush=True)
    finally:
        (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(out/'report.json',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',type=Path)
    main(parser.parse_args().stage)
