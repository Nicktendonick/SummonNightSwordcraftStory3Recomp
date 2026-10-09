"""Short isolated input-driven playtest of the installed Guard test engines.

No screenshots or framebuffer comparisons. Original states/settings stay read-only.
State audits are separate from real-window presentation/audio measurements.
"""
from collections import Counter
import argparse
import json
from pathlib import Path
import re
import subprocess
import time
from types import SimpleNamespace

import benchmark_presentation_filters as bench
from validate_guard_experiment import Session

ROOT, OWNER = bench.ROOT, bench.OWNER
PACKAGE = OWNER.parent/'OldSCS3Portables/Guard Experiment 20260929-023325-a2fa'
EXE = PACKAGE/'Runtime/Swordcraft3CustomRendererBeta.exe'
ACCEPTED = ROOT.parent/'custom-renderer'


def sequence(*segments):
    return [key for key, count in segments for _ in range(count)]


def cases():
    # Cancel any auto-battle inherited from the fixture before combat actions.
    # Exercise R/jump/pause before attacking: this enemy can die very quickly.
    actions = sequence((1023,8),(1019,30),(1023,15),
        (767,3),(1023,18),(1019,24),(1023,12),
        (959,12),(1023,45),(1015,3),(1023,45),(1015,3),(1023,30),
        (1007,20),(991,20), *([(1022,3),(1023,21)]*4),
        (1021,3),(1023,60),(1019,30),(1023,60))
    return [
        ('rocky-actions', bench.DEFAULT_STATE, actions, 'combat'),
        ('forest-actions', ACCEPTED/'validation/playtest-20260913-215538-152/frame-0000005817-1789350991388/state.gbas', actions, 'combat'),
        ('arena2-actions', ACCEPTED/'validation/playtest-20260921-115042-843/frame-0000009350-1790005934164/state.gbas', actions, 'combat'),
        ('spell-effect', ACCEPTED/'validation/playtest-20260923-194823-776/frame-0000005715-1790207395571/state.gbas',
            sequence((991,25),(1007,25),(991,25),(1007,25),(1023,110),(1019,30),(1023,360)), 'effect'),
        ('critical-effect', OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas',
            sequence((1023,210),(1019,30),(1023,360)), 'effect'),
        ('victory-exit', ACCEPTED/'validation/playtest-20260913-215538-152/frame-0000037049-1789351389376/state.gbas',
            sequence((1023,60), *([(1022,3),(1023,30)]*4),(1023,120)), 'transition'),
        ('slot3-native-fallback', PACKAGE/'Save States/beta.state3',
            sequence((1023,15),(1015,3),(1023,40),(1019,30),(1023,40)), 'fallback'),
        ('mine-select', PACKAGE/'Save States/beta.state5',
            sequence((1023,30),(1019,3),(1023,80)), 'field'),
    ]


def audit(out, state, keys):
    session = Session(out, True, EXE, diagnostic_env={
        'SWORDCRAFT3_STATE_TRACE':'1', 'SWORDCRAFT3_CUSTOM_AUDIT':'1',
        'SWORDCRAFT3_GUARD_TRACE':'0'})
    rows = []
    try:
        session.call('savestate_load', path=str(state))
        # Replays use the PPU's absolute clock, not an inferred TCP step count.
        start_frame = session.call('ppu_state')['frame']
        for key in keys:
            row = session.step(key)
            player = bytes.fromhex(row['player'])
            row['height'] = int.from_bytes(player[0x18c:0x190], 'little')
            rows.append(row)
    finally:
        session.close()
        (out/'frames.json').write_text(json.dumps(rows))
    log = (out/'stderr.log').read_text(errors='replace')
    compositions = [bench.values(line) for line in re.findall(r'\[sc3:composition\] ([^\r\n]+)',log)]
    states = [dict(re.findall(r'(\w+)=(\S+)',line)) for line in re.findall(r'\[sc3:state-frame\] ([^\r\n]+)',log)]
    field = [dict(re.findall(r'(\w+)=(\S+)',line)) for line in re.findall(r'\[sc3:field-frame\] ([^\r\n]+)',log)]
    owned = [r for r in compositions if r.get('complete_owner')==1]
    assert all(r['rows']==160 and r['center']==38400 and r['extended']==23040 for r in owned)
    # The actor buffer is repurposed after combat. Do not call arbitrary field
    # bytes a stuck Guard, AI mode, animation or equipped ability.
    live = [r for r in rows if r['root']==0x03000000 and 3<=r['phase']<=7]
    result = dict(frames=len(rows), start_frame=start_frame,
        guard_frames=sum(r['guard'] for r in live),
        final_guard=rows[-1]['guard'] if rows[-1] in live else None,
        modes=dict(Counter(r['mode'] for r in live)),
        phases=dict(Counter(r['phase'] for r in rows)),
        auto_modes=dict(Counter(r['auto'] for r in live)),
        animations=sorted({r['animation'] for r in live}),
        height_values=len({r['height'] for r in live}),
        slots=sorted({r['slot'] for r in live}),
        effect_kinds=dict(Counter(r['effect_kind'] for r in live)),
        live_arenas=sorted({r['arena'] for r in live}),
        complete_combat_frames=len(owned), affine_frames=sum(r['affine_rows']>0 for r in owned),
        battle_reasons=dict(Counter(r['reason'] for r in states)),
        arenas=sorted({int(r['arena']) for r in states if r['active']=='1'}),
        field_reasons=dict(Counter(r['reason'] for r in field)),
        state_restore_succeeded=True, clean_exit=session.process.returncode==0)
    assert result['clean_exit']
    (out/'summary.json').write_text(json.dumps(result,indent=2))
    return result


def write_replay(path, keys, start_frame):
    entries = ['# gbarecomp-keyinput-v1','0,0x03ff']
    last = None
    for index, key in enumerate(keys):
        if key != last:
            entries.append(f'{start_frame+index},0x{key:04x}')
            last = key
    path.write_text('\n'.join(entries)+'\n',encoding='ascii')


def menu_playtest(out, state, trace):
    out.mkdir()
    (out/'Settings').mkdir()
    (out/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
    (out/'Save States').mkdir()
    env = bench.environment(out,384,trace)
    env.update(SWORDCRAFT3_SELECT_GUARD='1', SWORDCRAFT3_GUARD_TRACE='0',
        GBARECOMP_ASSIST_SCRIPT='30:menu_auto_on;60:menu_open;75:menu_probe;90:menu_pause;105:menu_probe;120:menu_resume;150:menu_probe;180:menu_auto_off;210:menu_open;240:menu_probe;270:menu_pause;300:menu_probe;330:menu_resume;360:menu_auto_on;390:menu_open;420:menu_aspect=0;435:menu_resume;465:menu_probe;480:menu_open;510:menu_aspect=1;525:menu_resume;555:menu_probe;570:menu_open;600:menu_aspect=2;615:menu_resume;645:menu_probe;660:menu_open;690:menu_scaling=0;705:menu_probe;720:menu_scaling=3;735:menu_probe;750:menu_resume;810:save9;870:load9;900:menu_probe')
    command = [str(EXE),'--window','--no-launcher','--scale','3','--frames','720',
        '--screen','raw','--view-width','240','--smooth-filter','1','--screen-effect','off',
        '--rom',str(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
        '--bios',str(OWNER/'gbarecomp/bios/gba_bios.bin'), '--load-state',str(state),
        '--save',str(out/'private.eep'),str(ROOT/'native-test.toml')]
    with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
        child = subprocess.Popen(command,cwd=out,env=env,stdout=stdout,stderr=stderr,
            creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            child.wait(timeout=60)
            assert child.returncode==0
        finally:
            if child.poll() is None:
                child.terminate(); child.wait(timeout=10)
    log=(out/'stderr.log').read_text(errors='replace')
    stdout=(out/'stdout.log').read_text(errors='replace')
    events={int(row['pump']):row for row in (dict(re.findall(r'(\w+)=(\S+)',line))
        for line in re.findall(r'\[runtime-menu\] ([^\r\n]+)',log))}
    identity=lambda p: tuple(events[p][k] for k in ('frame','cycles','pc'))
    assert identity(60)==identity(75)==identity(90)==identity(105)==identity(120), 'Auto/manual pause advanced guest'
    assert int(events[150]['frame'])>int(events[120]['frame']), 'Resume did not advance'
    assert int(events[240]['frame'])>int(events[210]['frame']), 'Run-behind-menu did not advance'
    assert identity(270)==identity(300)==identity(330), 'Manual pause advanced'
    for group in ((390,420,435),(480,510,525),(570,600,615),(660,690,705,720,735,750)):
        assert len({identity(p) for p in group})==1, 'Settings edits advanced paused guest'
    for selected,width in ((0,240),(1,284),(2,384)):
        assert f'selected={selected} applied={selected} host={width} guest=240' in log, ('Aspect not applied',selected)
    assert 'savestate_saved slot=9' in stdout and 'savestate_loaded slot=9' in stdout, 'Private save/restore not reached'
    stats=bench.values(re.findall(r'\[presentation-summary\] ([^\r\n]+)',log)[-1])
    assert stats['smooth']>0 and stats['fallback']==0
    result=dict(auto_pause=True,manual_pause=True,resume=True,run_behind_menu=True,
        aspect_widths=[240,284,384],smooth_toggle=True,private_save_restore=True,
        presentation=stats,clean_exit=True,performance='Menu pauses intentionally excluded from FPS claims')
    (out/'summary.json').write_text(json.dumps(result,indent=2))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--menu-only',action='store_true')
    parser.add_argument('--windows-from',type=Path,help='Reuse audited state schedules from a prior report')
    parser.add_argument('--screen',choices=('raw','classic'),default='raw')
    parser.add_argument('--effect',type=int,choices=(0,1,2),default=0)
    parser.add_argument('--strength',type=int,default=35)
    options=parser.parse_args()
    assert 0<=options.strength<=100 and not (options.menu_only and options.windows_from)
    out=ROOT/('validation/short-playtest-'+str(time.time_ns()))
    out.mkdir()
    selected=cases()
    protected={str(p):bench.digest(p) for p in [EXE,*(c[1] for c in selected),
        OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',OWNER/'gbarecomp/bios/gba_bios.bin']}
    player_files={p.relative_to(PACKAGE).as_posix():bench.digest(p)
        for p in PACKAGE.rglob('*') if p.is_file()}
    report=dict(executable=str(EXE),protected_sha256=protected,player_files_before=player_files,
        audits={},windows=[],menu=None,presentation_profile=dict(screen=options.screen,
            effect=options.effect,strength=options.strength,scaler='Smooth 2x',width=384))
    if options.windows_from:
        source=options.windows_from.resolve(strict=True)
        assert source.is_relative_to(ROOT/'validation')
        previous=json.loads(source.read_text())
        assert previous['protected_sha256']==protected, 'Audited executable/input identities changed'
        report['audits']=previous['audits']
        report['audit_source']=str(source)
    print('Evidence:',out,flush=True)
    original_env=bench.environment
    def environment(run,width,replay,audit=False):
        env=original_env(run,width,replay,audit)
        (run/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
        env.update(SWORDCRAFT3_SELECT_GUARD='1',SWORDCRAFT3_GUARD_TRACE='0',GBARECOMP_SMOOTH_PROFILE='1')
        return env
    try:
        for name,state,keys,kind in ([] if options.menu_only or options.windows_from else selected):
            result=audit(out/('audit-'+name),state,keys)
            report['audits'][name]=result
            print(name,json.dumps(result),flush=True)
            if kind=='combat':
                assert result['guard_frames']>0 and not result['final_guard'], (name,'Guard coverage/release')
                assert result['complete_combat_frames']>0, (name,'No wide combat')
                assert result['height_values']>1 and len(result['slots'])>1, (name,'Jump/R not reached')
                assert 3 in result['modes'] and result['modes'].get(2,0)>0, (name,'Native Start pause not reached')
            if kind=='effect': assert result['complete_combat_frames']>0, (name,'No effect-scene combat')
            if kind=='transition': assert result['complete_combat_frames']>0 and len(result['phases'])>1
            (out/'report.json').write_text(json.dumps(report,indent=2))
        bench.environment=environment
        for name,state,keys,kind in ([] if options.menu_only else selected):
            if kind not in ('combat','effect'): continue
            trace=out/(name+'.trace')
            write_replay(trace,keys,report['audits'][name]['start_frame'])
            args=SimpleNamespace(exe=EXE,state=state,rom=OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',
                bios=OWNER/'gbarecomp/bios/gba_bios.bin',frames=len(keys),warmup_frames=60,
                strength=options.strength,screen=options.screen)
            mode=('smooth-'+('off','lcd','crt')[options.effect],3,options.effect)
            run=bench.execute(args,out/('window-'+name),384,mode,trace)
            run['case']=name
            report['windows'].append(run)
            print(name,f"{run['fps']:.2f} FPS, p95 {run['gap_ms_p95']:.2f} ms, filter fallbacks {run['presentation']['fallback']}",flush=True)
            (out/'report.json').write_text(json.dumps(report,indent=2))
        if not options.windows_from:
            neutral=out/'neutral.trace'
            neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n',encoding='ascii')
            report['menu']=menu_playtest(out/'menu',bench.DEFAULT_STATE,neutral)
            print('Menu, aspect switching and private save/restore passed',flush=True)
    finally:
        bench.environment=original_env
        report['inputs_unchanged']=all(bench.digest(Path(p))==h for p,h in protected.items())
        report['player_files_unchanged']=player_files=={p.relative_to(PACKAGE).as_posix():bench.digest(p)
            for p in PACKAGE.rglob('*') if p.is_file()}
        (out/'report.json').write_text(json.dumps(report,indent=2))
        assert report['inputs_unchanged'] and report['player_files_unchanged']


if __name__=='__main__': main()
