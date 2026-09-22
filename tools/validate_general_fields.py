"""Run paired 12:5 TCP regressions for the source-backed field experiment."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--objects',choices=('native','enabled'),default='native')
    p.add_argument('--inventory',type=Path,default=ROOT/'validation/field-provenance-20260922/all-captures.json')
    a=p.parse_args()
    output=a.output.resolve()
    assert output.is_relative_to(ROOT/'validation') and not output.exists()
    output.mkdir(parents=True)
    inventory=json.loads(a.inventory.read_text())['records']
    fields={}
    for record in inventory:
        if record.get('state',{}).get('control')=='free' and record.get('all_sources_authenticated'):
            assets=tuple(x['asset'] for x in record['layers'])
            fields.setdefault(assets,record['state_file'])
    cases=[(f'field-{assets[0]}',state,'1023:6,991:12,1023:6,1007:12','field') for assets,state in fields.items()]
    scripted=[x for x in inventory if x.get('state',{}).get('control')=='scripted-or-blocked']
    cases += [(f'script-{n}',x['state_file'],'1023:10,1022:1,1023:30','script') for n,x in enumerate(scripted)]
    battle=ROOT/'validation/playtest-20260921-152503-440/frame-0000020742-1790018925942/state.gbas'
    cases.append(('battle',str(battle),'1023:6,767:3,1023:15','battle'))
    # Menu request and return: assess actual state changes, not appearance.
    cases.append(('menu',next(iter(fields.values())),'1023:6,1015:1,1023:40,1021:1,1023:40','menu'))
    summary=[]
    for name,state,sequence,kind in cases:
        for version,exe in [('before',a.before.resolve()),('after',ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')]:
            args=[sys.executable,'-B',str(ROOT/'tools/probe_battle_state.py'),'--state',state,
                  '--output',str(output/(name+'-'+version)),'--exe',str(exe),
                  '--sequence',sequence,'--compact','--objects',a.objects,'--field-audit']
            if version=='after': args.append('--general-fields')
            subprocess.run(args,check=True,creationflags=subprocess.CREATE_NO_WINDOW)
        old=output/(name+'-before'); new=output/(name+'-after')
        x=json.loads((old/'frames.json').read_text()); y=json.loads((new/'frames.json').read_text())
        assert len(x)==len(y) and len(x)>1
        for arow,brow in zip(x,y):
            for key in ('frame','key','hashes','field','battle'):
                assert arow[key]==brow[key],(name,brow['index'],key)
        frames=[dict(re.findall(r'(\w+)=([^ ]+)',line)) for line in (new/'stderr.log').read_text().splitlines()
                if '[sc3:field-frame]' in line]
        assert frames
        old_frames=[dict(re.findall(r'(\w+)=([^ ]+)',line)) for line in (old/'stderr.log').read_text().splitlines()
                    if '[sc3:state-frame]' in line]
        assert len(old_frames)==len(frames)
        if kind!='battle':
            # Walking may itself trigger an event. Preserve the old native
            # framing during that transition instead of demanding constant wide.
            assert all(f['completed']==g['completed'] and f['wide']==g['wide']
                       for f,g in zip(frames,old_frames)),(name,'framing changed')
        if kind=='field':
            assert any(f['wide']=='1' for f in frames),(name,'never widened')
            assert all(f['wide']=='1' or f['reason']=='lake-player-control' for f in frames[1:]),(name,frames)
            assert max(int(f['decodes']) for f in frames)==1,(name,'repeated decompression')
        elif kind=='battle':
            assert all(f['valid']=='0' and f['wide']=='0' for f in frames)
        elif kind=='script':
            assert all(f['wide']=='0' for f in frames)
        summary.append(dict(case=name,frames=len(x),guest_state_unchanged=True,
                            widened=sum(f['wide']=='1' for f in frames),
                            reasons=sorted({f['reason'] for f in frames}),
                            source_decodes=max(int(f['decodes']) for f in frames)))
        (output/'report.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
        print(json.dumps(summary[-1]),flush=True)
    print('PASS paired state regression',sum(x['frames'] for x in summary),'frames',flush=True)


if __name__=='__main__': main()
