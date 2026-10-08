"""Private state/input validation of opt-in two-sided battle framing. No pixels."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time

from validate_guard_experiment import Session
from validate_translation105 import ROOT, ROM

EXE=ROOT/'build-native/Swordcraft3Translation105.exe'
FIXTURES=ROOT/'validation/arena-audit-1790800955778446100'

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def exercise(arena,width,enabled,out,exe=EXE):
    fixture=FIXTURES/f'arena-{arena:02d}/combat-ready.gbas'
    protected=digest(fixture)
    session=Session(out,True,exe,ROM,camera=enabled,diagnostic_env={
        'SWORDCRAFT3_CUSTOM_HOST_WIDTH':str(width),'SWORDCRAFT3_STATE_TRACE':'1',
        'SWORDCRAFT3_GUARD_TRACE':'0'})
    rows=[]
    try:
        session.call('savestate_load',path=str(fixture))
        for label,keys,count in [('settle',0x3ff,4),('left',0x3df,180),('guard',0x3fb,12),
                                  ('right',0x3ef,240),('jump-right',0x3af,30),
                                  ('right',0x3ef,100),('pause',0x3f7,1),('paused',0x3ff,8),
                                  ('resume',0x3f7,1),('release',0x3ff,12)]:
            for _ in range(count):
                r=session.step(keys)
                ram=bytes.fromhex(session.call('read_iwram',addr=0x1a98,len=12)['data'])
                p=bytes.fromhex(r['player'])
                rows.append(dict(label=label,phase=r['phase'],mode=r['mode'],pause=r['pause'],
                    x=int.from_bytes(p[0x188:0x18c],'little',signed=True),
                    y=int.from_bytes(p[0x18c:0x190],'little',signed=True),guard=r['guard'],
                    camera=int.from_bytes(ram[:2],'little',signed=True),camera_mode=ram[10],
                    player=r['player'],hashes=r['hashes']))
        # Restore must discard any retained framing immediately.
        session.call('savestate_load',path=str(fixture))
        for _ in range(3): session.step()
    finally:
        session.close()
    assert digest(fixture)==protected
    log=(out/'stderr.log').read_text(errors='replace')
    traces=[{k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
            for line in re.findall(r'\[sc3:battle-camera\] (completed=[^\r\n]+)',log)]
    active=[t for t in traces if t['active']]
    expected=enabled and width>240 and arena in (0,2,3,7)
    assert bool(active)==expected,(arena,width,enabled,'active',len(active))
    for t in active:
        span=368; shown=min(width,span)
        assert t['begin']==(width-shown)//2 and t['end']==t['begin']+shown
        assert 0<=t['origin']<=span-shown
        assert t['begin']-t['anchor']+t['camera']==t['origin']
        assert t['end']-t['anchor']+t['camera']<=span
    if expected:
        assert any(t['camera']==0 for t in active),'Left camera limit not reached'
        assert any(t['camera']==128 for t in active),'Right camera limit not reached'
    result=dict(arena=arena,width=width,enabled=enabled,active_frames=len(active),
                cameras=sorted({t['camera'] for t in active}),clean_exit=session.process.returncode==0,
                source_unchanged=True,frames=len(rows))
    assert result['clean_exit']
    (out/'frames.json').write_text(json.dumps(rows)+'\n')
    (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)
    return rows,result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arenas',type=int,nargs='+',default=[0,2,3,7,1])
    p.add_argument('--widths',type=int,nargs='+',default=[384,284,240])
    a=p.parse_args()
    out=ROOT/'validation'/('battle-camera-'+str(time.time_ns())); out.mkdir()
    print('EVIDENCE='+str(out),flush=True)
    results=[]
    for arena in a.arenas:
        for width in a.widths:
            baseline=None
            for enabled in (False,True):
                rows,result=exercise(arena,width,enabled,out/f'arena-{arena}-{width}-{int(enabled)}')
                if baseline is None: baseline=rows
                else:
                    # Additional offscreen OBJ submissions are allowed. Compare
                    # exact player/gameplay fields, not the presentation buffers.
                    keys=('player','phase','mode','pause','camera','camera_mode')
                    assert [{k:r[k] for k in keys} for r in baseline]==[{k:r[k] for k in keys} for r in rows]
                    if width==240 or arena not in (0,2,3,7):
                        assert [r['hashes'] for r in baseline]==[r['hashes'] for r in rows]
                    result['gameplay_matches_current']=True
                results.append(result)
    (out/'REPORT.json').write_text(json.dumps(dict(passed=True,cases=results),indent=2)+'\n')
    print('PASS: both boundaries, source coordinates, gameplay, fallback, restore',flush=True)

if __name__=='__main__': main()
