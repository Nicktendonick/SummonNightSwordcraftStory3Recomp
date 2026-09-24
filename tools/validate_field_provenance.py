"""Validate paired field-audit TCP runs using guest memory and cycles only."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    a=p.parse_args()
    root=a.root.resolve()
    if not root.is_relative_to(ROOT/'validation'):
        p.error('Evidence must stay in private validation')
    summary=[]
    for case,control in [('field','free'),('dialogue','scripted-or-blocked'),('battle','not-field-owned')]:
        old=root/(case+'-baseline')
        new=root/(case+'-live')
        oi=json.loads((old/'identity.json').read_text())
        ni=json.loads((new/'identity.json').read_text())
        for key in ('state_sha256','executable_sha256','sequence','host_width','objects','frames'):
            assert oi[key]==ni[key],(case,key)
        assert ni['host_width']==384 and ni['objects']=='native'
        assert not oi['field_audit'] and ni['field_audit']
        for identity in (oi,ni):
            for key in ('state','executable'):
                assert hashlib.sha256(Path(identity[key]).read_bytes()).hexdigest()==identity[key+'_sha256']
        before=json.loads((old/'frames.json').read_text())
        after=json.loads((new/'frames.json').read_text())
        assert len(before)==len(after)==ni['frames'] and len(after)>1
        endpoint_count=0
        for x,y in zip(before,after):
            for key in ('index','frame','key','battle','hashes'):
                assert x[key]==y[key],(case,y['index'],key)
            assert y['field']['control']==control,(case,y['index'],y['field'])
            assert y['field']['owned']==(case!='battle')
            if 'field_provenance' in y:
                endpoint_count+=1
                record=y['field_provenance']
                assert record['ownership_valid'] and not record['render_authorized']
                if case!='battle': assert record['all_sources_authenticated']
                else: assert 'layers' not in record
        assert endpoint_count==2
        summary.append(dict(case=case,frames=len(after),control=control,
            all_recorded_guest_hashes_and_cycles_unchanged=True,endpoint_checks=endpoint_count))
    report=root/'live-verification.json'
    report.write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary,indent=2))


if __name__=='__main__': main()
