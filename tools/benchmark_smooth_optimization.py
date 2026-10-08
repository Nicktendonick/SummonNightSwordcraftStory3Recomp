"""Matched old/new presentation benchmark. Private inputs stay read-only.

No scene/image comparisons. Live input is excluded and every child has private
settings/saves. Run serially with no other game instance or build in progress.
"""
import argparse
import csv
import json
from pathlib import Path
import statistics
import time
from types import SimpleNamespace
import benchmark_presentation_filters as b


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old',type=Path,default=b.OWNER.parent/'OldSCS3Portables/Guard Experiment 20260929-023325-a2fa/Runtime/Swordcraft3CustomRendererBeta.exe')
    parser.add_argument('--new',type=Path,default=b.ROOT/'build-native/Swordcraft3CustomRendererBeta.exe')
    parser.add_argument('--frames',type=int,default=600)
    parser.add_argument('--widths',type=int,nargs='+',choices=[240,384],default=[240,384])
    parser.add_argument('--passes',type=int,choices=[1,2],default=2)
    args=parser.parse_args()
    assert 120<=args.frames<=1800
    old=args.old.resolve(strict=True)
    args.new=args.new.resolve(strict=True)
    out=b.ROOT/('validation/smooth-optimization-'+str(time.time_ns()))
    out.mkdir()
    settings=SimpleNamespace(frames=args.frames,warmup_frames=90,strength=35,
        state=b.DEFAULT_STATE,rom=b.OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',
        bios=b.OWNER/'gbarecomp/bios/gba_bios.bin')
    protected={str(p):b.digest(p) for p in (old,args.new,settings.state,settings.rom,settings.bios)}
    neutral=out/'neutral.trace'
    neutral.write_text('# gbarecomp-keyinput-v1\n0,0x03ff\n',encoding='ascii')
    original=b.environment
    def environment(run,width,replay,audit=False):
        env=original(run,width,replay,audit)
        (run/'Settings/guard.ini').write_text('[Launcher]\nselect_guard = 1\n', encoding='utf-8')
        env.update(SWORDCRAFT3_SELECT_GUARD='1',SWORDCRAFT3_GUARD_TRACE='0',GBARECOMP_SMOOTH_PROFILE='1')
        return env
    b.environment=environment
    report=dict(inputs_sha256=protected,frames=args.frames,warmup=90,runs=[],audits=[])
    print('Evidence:',out,flush=True)
    try:
        for label,exe in [('old',old),('new',args.new)]:
            settings.exe=exe
            for width in args.widths:
                report['audits'].append(b.execute(settings,out/f'audit-{label}-{width}',width,b.MODES[0],neutral,True))
        cases=[(label,exe,width,mode) for width in args.widths
            for mode in (b.MODES[0],b.MODES[2]) for label,exe in [('old',old),('new',args.new)]]
        for repeat in range(args.passes):
            for label,exe,width,mode in (cases if repeat==0 else reversed(cases)):
                settings.exe=exe
                print(label,width,mode[0],repeat+1,flush=True)
                run=b.execute(settings,out/f'{label}-{width}-{mode[0]}-{repeat}',width,mode,neutral)
                run.update(build=label,repeat=repeat+1)
                with (Path(run['log_directory'])/'frame-phase.csv').open() as stream:
                    phases=list(csv.DictReader(stream))[90:]
                run['mean_phase_ms']={key:statistics.mean(float(row[key]) for row in phases)/1000
                    for key in ('guest_us','render_us','present_us','audio_us','pump_us','pacer_us')}
                log=(Path(run['log_directory'])/'stderr.log').read_text(errors='replace')
                run['smooth_profile']=[line for line in log.splitlines() if '[smooth-profile]' in line]
                report['runs'].append(run)
                (out/'report.json').write_text(json.dumps(report,indent=2))
                print(f"{run['fps']:.2f} FPS p95={run['gap_ms_p95']:.2f}ms; {run['smooth_profile']}",flush=True)
    finally:
        report['inputs_unchanged']=all(b.digest(Path(p))==h for p,h in protected.items())
        (out/'report.json').write_text(json.dumps(report,indent=2))
        assert report['inputs_unchanged']


if __name__=='__main__': main()
