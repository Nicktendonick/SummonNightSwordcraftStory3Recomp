"""Input-only startup route finding for arena fixtures; private saves isolated.

Does not certify battery compatibility. Reports guest task/VM state, not images.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
from validate_translation105 import ROOT, OWNER, ROM
from validate_guard_experiment import Session
from audit_field_provenance import field_state


def run(args):
    out = ROOT/'validation'/('arena-route-'+str(time.time_ns()))
    out.mkdir()
    print(out,flush=True)
    protected = hashlib.sha256(args.battery.read_bytes()).hexdigest() if args.battery else None
    session = Session(out/'run',False,ROOT/'build-native/Swordcraft3Translation105.exe',ROM,
        diagnostic_env={'SWORDCRAFT3_STATE_TRACE':'1'},battery_source=args.battery)
    records=[]
    try:
        if args.state: session.call('savestate_load',path=str(args.state.resolve()))
        for cycle in range(args.cycles):
            key = int(args.key,0) if args.key else (0x3f7 if cycle%40==0 else 0x3fe)
            session.call('run_frames',n=args.held_frames,keyinput=key)
            session.call('run_frames',n=30-args.held_frames,keyinput=0x3ff)
            if cycle%10==0 or cycle==args.cycles-1:
                i=bytes.fromhex(session.call('read_iwram',addr=0,len=0x8000)['data'])
                e=bytes.fromhex(session.call('read_ewram',addr=0,len=0x40000)['data'])
                u32=lambda a:int.from_bytes(i[a:a+4],'little')
                try: field=field_state(e,i)
                except ValueError as err: field={'error':str(err)}
                row=dict(cycle=cycle,frame=session.call('frame')['frame'],field=field,
                    battle_root=hex(u32(0x6ac0)),phase=u32(0x6ab4),arena=i[0x1a94],
                    mode=i[0xc],held=int.from_bytes(i[0x594c:0x594e],'little'),
                    script=u32(0x6574),vm=i[0x6590:0x65c4].hex(),
                    registers=session.call('registers'),misses=session.call('misses'))
                records.append(row)
                print(json.dumps({k:row[k] for k in ('cycle','frame','field','battle_root','phase','mode')}),flush=True)
                session.call('savestate_save',path=str(out/f'checkpoint-{cycle:04d}.gbas'))
                reached = (field.get('owned') if args.until == 'field' else
                           field.get('control') == 'free' if args.until == 'free' else
                           row['battle_root']=='0x3000000')
                if reached:
                    print('GAMEPLAY OWNER REACHED',flush=True)
                    break
    finally:
        session.close()
        if args.battery: assert hashlib.sha256(args.battery.read_bytes()).hexdigest()==protected
        (out/'report.json').write_text(json.dumps(dict(records=records,source_battery_unchanged=True),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--battery',type=Path)
    p.add_argument('--state',type=Path)
    p.add_argument('--cycles',type=int,default=180)
    p.add_argument('--key')
    p.add_argument('--until', choices=('field','free','battle'), default='field')
    p.add_argument('--held-frames', type=int, default=3, choices=range(1,30))
    run(p.parse_args())
