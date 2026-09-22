"""Compare 12:5 field modes without requesting screenshots or pixel checks."""
import argparse
import hashlib
import json
from pathlib import Path
from benchmark_combat_tcp import run,ROOT

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--state',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
state=a.state.resolve(); out=a.output.resolve()
assert out.is_relative_to(ROOT/'validation') and not out.exists()
out.mkdir(parents=True)
digest=hashlib.sha256(state.read_bytes()).hexdigest()
results={}
for label,enabled in [('profiles',0),('general',1)]:
    results[label]=run(state,out/label,1,1,384,120,3,replay_check=0,phase_profile=0,
                       screenshots=False,general_fields=enabled)
    print(label,results[label]['median_ms_per_frame'],'ms/frame',flush=True)
assert results['profiles']['end_states']==results['general']['end_states']
assert hashlib.sha256(state.read_bytes()).hexdigest()==digest
(out/'report.json').write_text(json.dumps(dict(state_sha256=digest,executable_sha256=hashlib.sha256(
    (ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest(),
    guest_state_unchanged=True,runs=results),indent=2))
