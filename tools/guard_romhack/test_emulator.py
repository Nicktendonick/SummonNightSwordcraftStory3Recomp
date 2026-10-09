"""Independent GBA gameplay smoke tests; no images or PC runtime hooks.

All saves are copied into memory. A normal-input route reaches the first rocky
battle; each scenario restarts from this emulator's own fresh checkpoint.
"""
import json
from emulator import Emulator, ROOT
from build import identity

OUT = ROOT/'validation/guard-romhack'
BATTERY = ROOT.parents[1]/'release/Portable Camera Edge Test/Saves/battery.eep'

def brief(e):
    s=e.state(); s.pop('player'); return s

def reach_battle(e):
    for n in range(300):
        e.step(8 if n%40==0 else 1,3); e.step(0,27)
        p=int.from_bytes(e.read(0x03006b54,4),'little')
        if p==0x0200e000 and e.read(0x0300699c,4)==bytes.fromhex('00080002'):
            break
    else: raise AssertionError('Field route failed')
    e.step(0,30)
    field=e.save()
    for _ in range(41): e.step(64,29);e.step(0,1)
    for n in range(300):
        e.step(1,3);e.step(0,27)
        s=e.state()
        if s['root']=='0x3000000' and s['phase']==4 and s['mode']==2:
            assert s['pause']==0 and s['auto']==0
            e.step(0,8)
            return field,e.save()
    raise AssertionError('Combat route failed')

def sample(e,keys,n):
    rows=[]
    for _ in range(n): e.step(keys);rows.append(brief(e))
    return rows

def exercise(rom,label,patched,battery):
    e=Emulator(rom,battery)
    try:
        field,ready=reach_battle(e)
        (OUT/(label+'-ready.state')).write_bytes(ready)
        report=dict(rom=identity(rom.read_bytes()),route='ordinary inputs only',
                    entry=brief(e),scenarios={})
        guard_key=4 if patched else 2
        rows=sample(e,guard_key,120)+sample(e,0,20)
        assert all(r['guard'] and r['auto']==0 for r in rows[2:120]),(label,'hold',rows[:4])
        assert all(not r['guard'] for r in rows[122:]),(label,'release')
        report['scenarios']['hold_release']=rows
        e.load(ready)
        e.step(0x100,1);e.step(0,12)
        selected=e.state()['slot']
        rows=sample(e,guard_key,45)+sample(e,0,15)
        if patched:
            assert selected!=0,(label,'R did not select another ability')
            assert all(r['slot']==selected and r['auto']==0 for r in rows)
            assert all(r['guard'] for r in rows[2:45])
            assert all(not r['guard'] for r in rows[47:])
        report['scenarios']['native_R_then_guard']={'selected':selected,'rows':rows}
        e.load(ready);e.step(8,1);rows=sample(e,0,10)
        assert all(r['pause']!=0 for r in rows)
        e.step(8,1);rows+=sample(e,0,60)
        assert all(r['pause']==0 for r in rows[-8:])
        rows+=sample(e,guard_key,30)+sample(e,0,10)
        assert all(r['guard'] for r in rows[-38:-10])
        assert all(not r['guard'] for r in rows[-8:])
        report['scenarios']['pause_resume']=rows
        e.load(ready);rows=sample(e,64,10)+sample(e,guard_key,30)+sample(e,0,60)
        assert any(not r['guard'] for r in rows[10:20]),(label,'Guard ignored native airborne restriction')
        report['scenarios']['jump']=rows
        e.load(field);report['scenarios']['field_select_before']=brief(e)
        rows=sample(e,4,10)+sample(e,0,20)
        assert all(r['root']!='0x3000000' for r in rows)
        report['scenarios']['field_select']=rows
        report['passed']=True
        (OUT/(label+'-emulator.json')).write_text(json.dumps(report,indent=2)+'\n')
        print(label,'PASS; native R selects slot',selected,flush=True)
        return report
    finally: e.close()

def main():
    battery=BATTERY.read_bytes(); cases=[
        ('english-original',ROOT/'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba',False),
        ('english-patched',OUT/'english-v01/patched.gba',True),
        ('japanese-original',ROOT.parents[1]/'ROMs/swordcraft3_jp.gba',False),
        ('japanese-patched',OUT/'japanese-v01/patched.gba',True)]
    results={}
    for label,rom,patched in cases:results[label]=exercise(rom,label,patched,battery)
    for language in ('english','japanese'):
        a=results[language+'-original']['scenarios']['hold_release']
        b=results[language+'-patched']['scenarios']['hold_release']
        assert [(r['guard'],r['timer']) for r in a]==[(r['guard'],r['timer']) for r in b]
    assert BATTERY.read_bytes()==battery,'User save changed'
    summary=dict(passed=True,emulator='mGBA 0.10.5 / 26b7884bc25a5933960f3cdcd98bac1ae14d42e2',
        bios='Official GBA BIOS, private',both_languages=True,
        native_B_vs_Select_guard_timeline_parity=True,battery_unchanged=True,
        cases=list(results),limits=['one ordinary early battle, not a full game playthrough',
        'no physical console test', 'not a timing/performance benchmark'])
    (OUT/'emulator-report.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(summary)

if __name__=='__main__': main()
