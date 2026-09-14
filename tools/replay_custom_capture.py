"""Replay a private capture with native/wide parity and margin-transition diagnostics."""
import argparse, collections, hashlib, json, os
from pathlib import Path
from types import SimpleNamespace
import audit_widescreen_route as audit
from validate_custom_host import ROOT,OWNER,check,crop,margins

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--state',type=Path,required=True)
    inputs=p.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--trace',type=Path)
    inputs.add_argument('--tap-a',action='store_true',help='Synthetic dialogue advance every 12 frames')
    p.add_argument('--expect',choices=('wide','native','return-to-wide'))
    p.add_argument('--start',type=int,required=True)
    p.add_argument('--count',type=int,default=301)
    p.add_argument('--step',type=int,default=1)
    p.add_argument('--width',type=int,default=284)
    p.add_argument('--output-dir',type=Path,required=True)
    a=p.parse_args();out=a.output_dir.resolve()
    check(out.is_relative_to(ROOT/'validation'),'Use experimental validation')
    check(0<a.count<=6000 and a.step>0,'Invalid frame range')
    check(240<a.width<=480,'Invalid host width')
    out.mkdir(parents=True,exist_ok=False);runtime=out/'runtime';runtime.mkdir()
    # Reuse the executable in place: retained diagnostic binaries exhausted the
    # owner's disk. Never rebuild it while this comparison is running.
    executable=ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
    binary_hash=hashlib.file_digest(executable.open('rb'),'sha256').hexdigest()
    config=runtime/'game.toml';config.write_text('[save]\ntype="eeprom"\nsize="0x2000"\n')
    if a.tap_a:
        a.trace=runtime/'dialogue-advance.trace'
        a.trace.write_text('# gbarecomp-keyinput-v1\n0,0x03FF\n'+''.join(
            f'{f},0x03FE\n{f+2},0x03FF\n' for f in range(a.start,a.start+a.count,12)))
    original=hashlib.sha256(a.state.read_bytes()).hexdigest()
    os.environ.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',GBARECOMP_SELFHEAL_RECOMPILE='0',SWORDCRAFT3_CUSTOM_AUDIT='1')
    for key in ('GBARECOMP_VISIBLE_DEBUGGER','GBARECOMP_INPUT_RECORD','GBARECOMP_VIEW_WIDTH','GBARECOMP_WS_WIP',
                'GBARECOMP_WIDESCREEN','GBARECOMP_RESIZE_VIEW','SWORDCRAFT3_LAKE_EDGE_DATA','SWORDCRAFT3_LAKE_CAMERA_LIMITS'):
        os.environ.pop(key,None)
    frames=list(range(a.start,a.start+a.count,a.step));report={
        'passed':False,'executable_sha256':binary_hash,'snapshot_sha256':original}
    baseline=None;states0=None
    try:
        for width in (240,a.width):
            os.environ['SWORDCRAFT3_CUSTOM_RENDERER']='1' if width>240 else '0'
            os.environ['SWORDCRAFT3_CUSTOM_HOST_WIDTH']=str(width)
            args=SimpleNamespace(executable=executable,
                rom=OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',rom_sha1='bb2eebf98deb59bb6218442c2308bb5033ae2915',
                bios=OWNER/'gbarecomp/bios/gba_bios.bin',config=config,load_state=a.state,input_replay=a.trace,
                strict_static=False,start=a.start,end=frames[-1],step=a.step,timeout=600,wide_width=240)
            audit.capture_run(args,out/str(width),'native','composite',frames)
            raw=out/str(width)/'raw/native/composite';pictures=[];coverage=[]
            for f in frames:
                w,h,rgb=audit.read_png_rgb(raw/f'f_{f:06d}.png');check((w,h)==(width,160),'Dimensions')
                pictures.append(crop(rgb,w));coverage.append(any(margins(rgb,w)))
            states={r['frame']:r for r in map(json.loads,(raw/'state.jsonl').read_text().splitlines()) if a.start<=r['frame']<=frames[-1]}
            if width==240: baseline=pictures;states0=states
            else:
                check(baseline==pictures,'Native center changed');check(states0==states,'Guest state changed')
                log=(raw/'stderr.log').read_text(errors='replace');check('native mismatch=' not in log,'Replay mismatch')
                reasons=collections.Counter(line.split('reason=')[-1].split()[0] for line in log.splitlines() if '[sc3:host-decline]' in line)
                report.update(width=width,sampled_frames=len(frames),full_state_records=len(states),
                    native_pixels_identical=True,full_guest_state_identical=True,reasons=dict(reasons),
                    margin_changes=[{'frame':f,'wide':v} for i,(f,v) in enumerate(zip(frames,coverage)) if i==0 or v!=coverage[i-1]])
                if a.expect=='wide': check(all(coverage) and not reasons,'Intermittent fallback')
                if a.expect=='native': check(not any(coverage),'Unexpected widening')
                if a.expect=='return-to-wide':
                    check(not coverage[0] and coverage[-1],'Did not return from native to wide')
                    check(len(report['margin_changes'])==2,'Framing toggled during the dialogue')
        check(original==hashlib.sha256(a.state.read_bytes()).hexdigest(),'Snapshot changed')
        check(binary_hash==hashlib.file_digest(executable.open('rb'),'sha256').hexdigest(),'Executable changed during replay')
        report['passed']=True;print(json.dumps(report,indent=2),flush=True)
    finally: (out/'report.json').write_text(json.dumps(report,indent=2))

if __name__=='__main__': main()
