"""Alternating before/after headless timings; no images, replay, or guest changes."""
import argparse
import hashlib
import json
from pathlib import Path
from benchmark_combat_tcp import run, ROOT
from validate_full_field import cases

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--cases',nargs='+',help='Restrict to named field cases')
    a=p.parse_args()
    selected=[c for c in cases() if c[3]=='field']
    if a.cases: assert set(a.cases)<={c[0] for c in selected}, 'Unknown field case'
    out=a.output.resolve()
    assert out.is_relative_to(ROOT/'validation') and not out.exists()
    out.mkdir(parents=True)
    results=[]
    for name,state,_,kind in selected:
        if a.cases and name not in a.cases: continue
        digest=hashlib.sha256(state.read_bytes()).hexdigest()
        old=None
        # Reverse order in a second pair to expose host load/warm-up bias.
        for n,mode in enumerate(('before','after','after','before')):
            exe=a.before.resolve() if mode=='before' else ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
            r=run(state,out/f'{name}-{n}-{mode}',1,1,384,120,2,replay_check=0,phase_profile=0,
                  screenshots=False,general_fields=1,exe=exe,full_field=int(mode=='after'),full_combat=1)
            r.update(case=name,mode=mode,state_sha256=digest,order=n)
            if old is None: old=r['end_states']
            else: assert old==r['end_states'], (name,'endpoint state changed')
            results.append(r)
            (out/'report.json').write_text(json.dumps(results,indent=2))
            print(f'{name} {n} {mode}: {r["median_ms_per_frame"]:.3f} ms/frame',flush=True)
        assert hashlib.sha256(state.read_bytes()).hexdigest()==digest

if __name__=='__main__': main()
