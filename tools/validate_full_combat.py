"""Paired TCP control-state validation. Does not request or inspect images."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from probe_battle_state import ROOT, OWNER
from validate_spell_windows import traces

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cases',nargs='+')
    parser.add_argument('--verify-existing',action='store_true')
    args=parser.parse_args()
    out=args.output.resolve()
    assert out.is_relative_to(ROOT/'validation')
    accepted=ROOT.parent/'custom-renderer'
    rocky=OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000061510-1788964715395/state.gbas'
    critical=OWNER/'validation/visible-debugger/20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas'
    victory=accepted/'validation/playtest-20260913-215538-152/frame-0000037049-1789351389376/state.gbas'
    flare=accepted/'validation/playtest-20260923-194823-776/frame-0000005715-1790207395571/state.gbas'
    field=accepted/'validation/playtest-20260921-115042-843/frame-0000003773-1790005895680/state.gbas'
    cases=[('flare-turns',flare,'991:25,1007:25,991:25,1007:25',True),
           ('rocky-r',rocky,'1023:6,'+','.join(['767:3,1023:15']*8),True),
           ('rocky-jump',rocky,'1023:6,959:120,1023:60',True),
           ('critical',critical,'1023:90',True),
           ('victory-exit',victory,'1023:60,1022:3,1023:30,1022:3,1023:30,1022:3,1023:30,1022:3,1023:90',None),
           ('field-negative',field,'1023:60',False)]
    if args.cases:
        assert set(args.cases)<={c[0] for c in cases}
        cases=[c for c in cases if c[0] in args.cases]
    report=[]
    for name,state,sequence,positive in cases:
        for mode in ('accepted','full'):
            command=[sys.executable,'-B',str(ROOT/'tools/probe_battle_state.py'),
                '--state',str(state),'--output',str(out/name/mode),'--sequence',sequence,
                '--compact','--general-fields','--window-audit']
            if mode=='full': command.append('--full-combat')
            else: command+=['--exe',str(accepted/'build-native/Swordcraft3CustomRendererBeta.exe')]
            if not args.verify_existing: subprocess.run(command,check=True)
        old,new=[json.loads((out/name/mode/'frames.json').read_text()) for mode in ('accepted','full')]
        assert old==new and new, (name,'guest state/cycles changed')
        identities=[json.loads((out/name/mode/'identity.json').read_text()) for mode in ('accepted','full')]
        for key in ('state_sha256','sequence','host_width','objects','general_fields'):
            assert identities[0][key]==identities[1][key], (name,key)
        assert not identities[0]['full_combat'] and identities[1]['full_combat']
        old_raster,old_states=traces(out/name/'accepted')
        new_raster,new_states=traces(out/name/'full')
        assert old_raster==new_raster and old_states==new_states, (name,'raster/policy state changed')
        log=(out/name/'full/stderr.log').read_text(errors='replace')
        rows=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',m))
              for m in re.findall(r'\[sc3:composition\] ([^\n]+)',log)]
        owned=[r for r in rows if r['complete_owner']]
        for r in owned:
            assert r['rows']==160 and r['center']==240*160 and r['extended']==144*160, (name,r)
        if positive is True: assert owned, (name,'never owned a combat frame')
        if positive is False: assert not owned, (name,'combat renderer claimed field frame')
        ownership={r['completed']:bool(r['complete_owner']) for r in rows}
        for completed,state_row in new_states.items():
            if state_row['active']=='1' and state_row['wide']=='1':
                assert ownership.get(completed), (name,completed,'accepted combat still hybrid')
        if name=='victory-exit':
            assert owned and any(r['completed']>owned[-1]['completed'] and not r['complete_owner'] for r in rows), 'ownership not released'
        report.append(dict(case=name,frames=len(new),complete_combat_frames=len(owned),
            affine_frames=sum(r['affine_rows']>0 for r in owned),guest_state_unchanged=True))
        (out/'report.json').write_text(json.dumps(report,indent=2))
        print(json.dumps(report[-1]),flush=True)

if __name__=='__main__': main()
