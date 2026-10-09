"""Private, state-only arena loader tests for the released translation.

No game/renderer changes: vary script variable 0x1A1 in a copied snapshot at
battle lifecycle 2, BEFORE sub_080264CC reads it and loads the arena normally.
This tests each background with one encounter, not every enemy/story battle.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import time

from validate_translation105 import ROOT, OWNER, ROM
from validate_guard_experiment import Session
from inspect_scene_state import battle_assets
from audit_field_provenance import field_state

SHA1 = '06a9f4db52f40a7034dc1c74161a705f30edb858'
EXE = ROOT/'build-native/Swordcraft3Translation105.exe'


def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def u32(b,p): return struct.unpack_from('<I',b,p)[0]


def sections(blob):
    assert blob[:4] == b'GBAS' and blob[8:48].decode() == SHA1
    assert u32(blob,4) in (1,2)
    p=52; result={}
    for _ in range(u32(blob,48)):
        tag=bytes(blob[p:p+4]); size=u32(blob,p+4); p+=8
        assert tag not in result and size <= len(blob)-p
        result[tag]=(p,size); p+=size
    assert p==len(blob)
    return result


def selection_offset(blob):
    bus,size=sections(blob)[b'BUS0']; assert size>=0x48000
    i=bus+0x40000
    assert u32(blob,i+0x6ac0)==0x03000000 and u32(blob,i+0x6ab4)==2
    # 08012F60 reads signed byte gUnk_03006584[variable - 0x180].
    pointer=u32(blob,i+0x6584)+0x21
    if 0x02000000<=pointer<0x02040000: return bus+pointer-0x02000000
    assert 0x03000000<=pointer<0x03008000
    return i+pointer-0x03000000


def prepare(state,out):
    protected=digest(state)
    session=Session(out/'route',False,EXE,ROM)
    rows=[]
    try:
        session.call('savestate_load',path=str(state.resolve()))
        for n in range(1200):
            row=session.step(0x3fe if n%30<3 else 0x3ff)
            rows.append({k:row[k] for k in ('frame','root','phase','mode','arena')})
            if row['root']==0x03000000 and row['phase']==2:
                fixture=out/'before-arena-load.gbas'
                session.call('savestate_save',path=str(fixture))
                b=fixture.read_bytes(); offset=selection_offset(b)
                print('Fixture:',fixture,'natural arena:',b[offset],flush=True)
                return dict(fixture=str(fixture),sha256=digest(fixture),natural_arena=b[offset],
                            source_state=str(state),source_sha256=protected,rows=rows)
        raise RuntimeError('No pre-initialization boundary reached')
    finally:
        session.close()
        assert digest(state)==protected
        (out/'route.json').write_text(json.dumps(rows,indent=2)+'\n')


def inventory():
    rom=ROM.read_bytes(); assert hashlib.sha1(rom).hexdigest()==SHA1
    jp=(OWNER.parent/'Swordcraft Story 3 rom files/swordcraft3_jp.gba').read_bytes()
    spans=[(0x12f60,0x12fb8),(0x264cc,0x26a44),(0x2b95c,0x2bc90),
           (0x31420,0x31bc8),(0xb801cc,0xb80418)]
    audits=[]
    for start,end in spans:
        assert rom[start:end]==jp[start:end],hex(start)
        audits.append(dict(start=hex(start),end=hex(end),sha256=hashlib.sha256(rom[start:end]).hexdigest()))
    descriptors=[battle_assets(ROM,a) for a in range(21)]
    assert all(int(d['descriptor'][:2],16)==n for n,d in enumerate(descriptors[:17]))
    assert len({d['descriptor'] for d in descriptors[17:]})==1
    return dict(rom_sha1=SHA1,source_spans=audits,descriptors=descriptors)


def exercise(fixture,arena,out,change=True):
    original=fixture.read_bytes(); b=bytearray(original)
    offset=selection_offset(b)
    if change: b[offset]=arena
    assert all(x==y for n,(x,y) in enumerate(zip(original,b)) if n!=offset)
    target=out/'input.gbas'; out.mkdir(); target.write_bytes(b)
    session=Session(out/'run',True,EXE,ROM,diagnostic_env={
        'SWORDCRAFT3_STATE_TRACE':'1','SWORDCRAFT3_GUARD_TRACE':'0'})
    rows=[]; result=dict(arena=arena,selection_only=True,ready=False)
    def step(key,label):
        r=session.step(key); p=bytes.fromhex(r.pop('player'))
        r.update(label=label,hp=int.from_bytes(p[0xf4:0xf6],'little'),
                 height=int.from_bytes(p[0x18c:0x190],'little'))
        rows.append(r); return r
    try:
        session.call('savestate_load',path=str(target))
        for n in range(480):
            r=step(0x3ff,'loading')
            if r['root']==0x03000000 and r['phase']==4 and r['mode']==2:
                assert r['arena']==arena, (arena,r['arena'])
                result['ready']=True; break
        if result['ready']:
            session.call('savestate_save',path=str(out/'combat-ready.gbas'))
            result['ready_hashes']=session.call('state_hash')
            segments=[('settle',0x3ff,8),('left',0x3df,25),('right',0x3ef,25),
                ('jump',0x3bf,10),('land',0x3ff,45),('guard',0x3fb,30),('release',0x3ff,15),
                ('pause',0x3f7,1),('paused',0x3ff,15),('resume',0x3f7,1),('resumed',0x3ff,15)]
            segments += [('attack',0x3fe,3),('recover',0x3ff,25)]*4
            for label,key,count in segments:
                for _ in range(count): step(key,label)
        result['misses']=session.call('misses')
        session.call('savestate_save',path=str(out/'final.gbas'))
    except (AssertionError,RuntimeError,OSError,ValueError) as error:
        result['error']=str(error)
    finally:
        session.close()
        (out/'frames.json').write_text(json.dumps(rows)+'\n')
    log=(out/'run/stderr.log').read_text(errors='replace')
    traces=[dict(re.findall(r'(\w+)=(\S+)',line)) for line in re.findall(r'\[sc3:state-frame\] ([^\r\n]+)',log)]
    result.update(clean_exit=session.process.returncode==0,frames=len(rows),
        phases=dict(Counter(r['phase'] for r in rows)),
        active_wide_frames=sum(r['active']=='1' and r['wide']=='1' for r in traces),
        reasons=dict(Counter(r['reason'] for r in traces)),
        guard_frames=sum(r['guard'] and r['label']=='guard' for r in rows),
        release_guard_frames=sum(r['guard'] and r['label']=='release' for r in rows),
        paused_frames=sum(r['pause']!=0 and r['label']=='paused' for r in rows),
        resumed_frames=sum(r['pause']==0 and r['label']=='resumed' for r in rows),
        jump_heights=sorted({r['height'] for r in rows if r['label'] in ('jump','land')}),
        battle_input_state_changes=len({r['hashes']['iwram'] for r in rows}) if rows and 'iwram' in rows[0]['hashes'] else None)
    result['bounded_smoke_passed']=(result['ready'] and result['clean_exit'] and 'error' not in result
        and result['guard_frames']==30 and result['release_guard_frames']==0
        and result['paused_frames']==15 and result['resumed_frames']>0
        and len(result['jump_heights'])>1
        and bool(result['active_wide_frames'])==(arena in (0,2,3,7)))
    (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result.get(k) for k in ('arena','ready','clean_exit','frames','active_wide_frames','guard_frames','error')}),flush=True)
    return result


def finish_probe(state,out):
    """Use the game's native auto-battle toggle; no HP/enemy/AI memory edits."""
    session=Session(out/'run',False,EXE,ROM,diagnostic_env={'SWORDCRAFT3_GUARD_TRACE':'0'})
    rows=[]; reached_result=False; returned_field=False; protected=digest(state)
    try:
        session.call('savestate_load',path=str(state.resolve()))
        session.call('run_frames',n=2,keyinput=0x3fb)
        for n in range(160):
            session.call('run_frames',n=29,keyinput=0x3ff)
            r=session.step(0x3fe if reached_result and n%2==0 else 0x3ff)
            rows.append({k:r[k] for k in ('frame','root','phase','mode','auto','arena')})
            if r['root']==0x03000000 and r['phase'] in (5,6,7,8,9):
                reached_result=True
            if reached_result:
                i=bytes.fromhex(session.call('read_iwram',addr=0,len=0x8000)['data'])
                e=bytes.fromhex(session.call('read_ewram',addr=0,len=0x40000)['data'])
                if field_state(e,i).get('owned'):
                    returned_field=True; break
        session.call('savestate_save',path=str(out/'finish.gbas'))
    finally:
        session.close()
        assert digest(state)==protected
        (out/'finish.json').write_text(json.dumps(rows,indent=2)+'\n')
    result=dict(source=str(state),source_sha256=protected,observations=len(rows),
        result_reached=reached_result,field_returned=returned_field,clean_exit=session.process.returncode==0,
        phases=sorted({r['phase'] for r in rows}),source_unchanged=True)
    print(json.dumps(result),flush=True)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prepare',type=Path)
    p.add_argument('--fixture',type=Path)
    p.add_argument('--arenas',type=int,nargs='+',default=list(range(21)))
    p.add_argument('--finish',type=Path)
    p.add_argument('--finish-matrix',type=Path)
    args=p.parse_args()
    assert all(0<=arena<21 for arena in args.arenas)
    out=ROOT/'validation'/('arena-audit-'+str(time.time_ns())); out.mkdir()
    print(out,flush=True)
    report=inventory()
    if args.finish: report['finish']=finish_probe(args.finish,out)
    if args.finish_matrix:
        report['exits']=[]
        for arena in args.arenas:
            case=out/f'exit-{arena:02d}'; case.mkdir()
            r=finish_probe(args.finish_matrix/f'arena-{arena:02d}/combat-ready.gbas',case)
            r['arena']=arena; report['exits'].append(r)
            (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.prepare: report['prepare']=prepare(args.prepare,out)
    if args.fixture:
        protected=digest(args.fixture)
        report.update(fixture=str(args.fixture.resolve()),fixture_sha256=protected,executable_sha256=digest(EXE),cases=[])
        for arena in args.arenas:
            assert 0<=arena<21
            report['cases'].append(exercise(args.fixture,arena,out/f'arena-{arena:02d}'))
            (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        assert digest(args.fixture)==protected
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__': main()
