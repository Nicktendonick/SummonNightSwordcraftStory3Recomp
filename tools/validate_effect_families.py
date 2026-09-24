"""Paired 12:5 effect lifecycle regression; state/register evidence only."""
import argparse
import json
from pathlib import Path
from validate_dark_hole import CASES, digest, read, run_or_verify
from validate_spell_windows import traces
from probe_battle_state import ROOT

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    out=a.output.resolve(); before=a.before.resolve()
    assert out.is_relative_to(ROOT/'validation')
    after=ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
    report=dict(before_sha256=digest(before),after_sha256=digest(after),
                host_width=384,cases=[],pass_complete=False)
    for name,state,seq,_ in CASES:
        count=sum(int(s.split(':')[1]) for s in seq.split(','))
        for label,exe in [('before',before),('after',after)]:
            assert run_or_verify(state,exe,out/name/label,seq)['frames']==count
        olddir,newdir=out/name/'before',out/name/'after'
        assert read(olddir/'frames.json')==read(newdir/'frames.json')
        oldrows,oldstates=traces(olddir); rows,states=traces(newdir)
        assert len(states)==count-1 and states==oldstates
        assert rows.keys()==oldrows.keys()
        assert set(rows)=={(f,y) for f,s in states.items() if s['active']=='1' for y in range(160)}
        policies=set(); masks=set()
        for key,row in rows.items():
            old=oldrows[key]
            assert {k:v for k,v in row.items() if k not in ('margin_layers','effect_policy')}==old, key
            policies.add(row['effect_policy']); masks.add(row['margin_layers'])
            assert row['margin_layers'] in (19,23) and not row['margin_layers']&8
            if row['y']<int(states[key[0]]['top']) or row['y']>=125:
                assert row['margin_layers']==19
            if row['effect_policy']==0:
                assert row['margin_layers']==19
        for s in states.values():
            if s['active']=='1': assert s['wide']=='1' and s['reason']=='none'
        for f in range(count):
            for region in ('iwram','oam'):
                assert (olddir/f'{f:04d}-{region}.bin').read_bytes()==(newdir/f'{f:04d}-{region}.bin').read_bytes()
        case=dict(case=name,frames=count,policies=sorted(policies),masks=sorted(masks),
                  unchanged_guest_state=True,unchanged_captured_registers=True,continuous_owned_widescreen=True)
        report['cases'].append(case); print(json.dumps(case),flush=True)
    for label in ('before','after'):
        original=out/'reported'/label
        rr,ss=traces(original)
        for repeat in ('repeat2','repeat3'):
            assert read(out/repeat/label/'frames.json')==read(original/'frames.json')[:60]
            assert traces(out/repeat/label)==({k:v for k,v in rr.items() if k[0]<60},
                                             {k:v for k,v in ss.items() if k<60})
    report.update(pass_complete=True,paired_frames=sum(c['frames'] for c in report['cases']),
                  limits='Existing captured effect families. Unknown formats/BG3 are synthetic-test coverage, not newly played spells.')
    (out/'report.json').write_text(json.dumps(report,indent=2))
    print('PASS',report['paired_frames'],'paired effect frames')

if __name__=='__main__':
    main()
