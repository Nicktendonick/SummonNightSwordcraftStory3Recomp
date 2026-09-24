"""Source/ROM dispatch inventory. No graphics, guest execution, or ROM output."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from probe_battle_state import ROOT, OWNER

FAMILIES = [
    ([0], '0803DB60', 'reset to default effect'),
    ([1], '0803A8EC', 'regular default canvas lifecycle'),
    ([2], '0803CA40', 'data-driven script: OBJ, regular/affine BG2, regular BG3'),
    ([3, 4, 5], '0803B5F0', 'casting: regular kinds 3/4, affine kind 5'),
    ([6], '0803AFA8', 'bounded 128 affine, priority 2'),
    ([7], '0803BDC8', 'bounded 256 affine critical impact'),
    ([8], '0803D714', 'default cleanup branch'),
    ([9, 10, 11, 12], '0803BFFC', 'bounded 128 affine: scale/rotation variants'),
    ([13], '0803C2F8', 'actor/status sequence with palette tasks'),
    ([14], '0803C618', 'palette-task sequence'),
    ([15], '0803C78C', 'actor sequence with palette tasks'),
]
BRANCHES = [0x39250,0x39256,0x392da,0x39296,0x39296,0x39296,0x39262,0x3925c,
            0x392fc,0x3928e,0x3928e,0x3928e,0x3928e,0x392e0,0x392e6,0x392ec]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    out=parser.parse_args().output.resolve()
    assert out.is_relative_to(ROOT/'validation')
    source=OWNER/'references/csm3/asm/code_attribute.s'
    text=source.read_text()
    entries=list(re.finditer(r'\bthumb_func_start (\w+)\s*\n\1: @ 0x([0-9A-Fa-f]+)',text))
    functions={m[1]:(m,entries[n+1] if n+1<len(entries) else None) for n,m in enumerate(entries)}
    jp=(OWNER/'roms/swordcraft3_jp.gba').read_bytes()
    beta=(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba').read_bytes()
    for rom in (jp,beta):
        assert list(struct.unpack_from('<16I',rom,0x39210))==[0x08000000+x for x in BRANCHES]
    reviewed=['080391E8']+[a for _,a,_ in FAMILIES]+[
        '0803B9C8','0803BB88','0803BF6C','0803CE14','0803CF68','0803D1FC','0803D3C8','0803D5A4','0803D68C']
    records=[]
    for address in sorted(set(reviewed)):
        m,end=functions['sub_'+address]
        start=int(m[2],16)-0x08000000
        stop=int(end[2],16)-0x08000000
        assert stop>start
        assert jp[start:stop]==beta[start:stop], ('changed routine',address)
        body=text[m.start():end.start()]
        records.append(dict(address=address,end=f'{stop+0x08000000:08X}',
            sha256=hashlib.sha256(jp[start:stop]).hexdigest(),
            direct_calls=sorted(set(re.findall(r'\bbl (\w+)',body)))))
    report=dict(dispatch='080391E8',table='08039210',kind_offset='owner+044E',
        stage_offset='owner+044F',all_16_dispatch_entries_accounted_for=True,
        families=[dict(kinds=k,handler=a,role=r) for k,a,r in FAMILIES],routines=records,
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        limits='Source inventory, not every script opcode/resource or gameplay spell tested. Labels are technical roles, not localized spell names.')
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2))
    print(f'PASS: 16 dispatch entries; {len(records)} reviewed routines match JP/beta; report {out}')

if __name__=='__main__':
    main()
