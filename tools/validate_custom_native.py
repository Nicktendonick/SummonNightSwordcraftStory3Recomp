"""Isolated native stock/custom integration; private owner captures required."""
import argparse, hashlib, json, os, shutil, sys, re
from pathlib import Path
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1]
OWNER=ROOT.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import audit_widescreen_route as audit

def check(ok,message):
    if not ok: raise RuntimeError(message)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args();out=args.output_dir.resolve()
    check(out.is_relative_to(ROOT/'validation'),'Use experimental validation')
    out.mkdir(parents=True,exist_ok=False)
    runtime=out/'runtime';runtime.mkdir()
    for name in ('Swordcraft3CustomRendererBeta.exe','SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll'):
        shutil.copyfile(ROOT/'build-native'/name,runtime/name)
    shutil.copytree(ROOT/'build-native/assets',runtime/'assets')
    shutil.copyfile(OWNER/'validation/opening-lake-pilot-20260909/extension-check-1/runtime/SummonNightSwordcraftStory3RecompBeta.exe',runtime/'previous.exe')
    config=runtime/'game.toml';config.write_text('[save]\ntype="eeprom"\nsize="0x2000"\n')
    trace=runtime/'released.trace';trace.write_text('# gbarecomp-keyinput-v1\n0,0x03FF\n')
    os.environ.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',GBARECOMP_SELFHEAL_RECOMPILE='0')
    for key in ('GBARECOMP_VISIBLE_DEBUGGER','GBARECOMP_INPUT_RECORD','GBARECOMP_VIEW_WIDTH',
                'GBARECOMP_WIDESCREEN','GBARECOMP_RESIZE_VIEW','SWORDCRAFT3_LAKE_EDGE_DATA','SWORDCRAFT3_LAKE_CAMERA_LIMITS',
                'SWORDCRAFT3_CUSTOM_HOST_WIDTH'):
        os.environ.pop(key,None)
    c=OWNER/'validation/visible-debugger'
    cases=[('lake',c/'20260827-130539-beta/captures/frame-0000019663-1787850529742/state.gbas',19665),
           ('battle',c/'20260827-130539-beta/captures/frame-0000004980-1787850405335/state.gbas',4982),
           ('critical',c/'20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas',9935),
           ('dialogue',c/'20260909-202234-658-beta/captures/frame-0000005967-1788999820512/state.gbas',5969)]
    report=dict(passed=False,cases=[],limitations=['Native capture/replay bridge, not independent pixel-kernel validation or widescreen acceptance.'])
    try:
        for name,state,start in cases:
            source_hash=hashlib.sha256(state.read_bytes()).hexdigest()
            frames=list(range(start,start+91,15));images={};states={};stats=[]
            for label in ('previous','off','on'):
                os.environ['SWORDCRAFT3_CUSTOM_RENDERER']='1' if label=='on' else '0'
                params=SimpleNamespace(executable=runtime/('previous.exe' if label=='previous' else 'Swordcraft3CustomRendererBeta.exe'),
                    rom=OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',rom_sha1='bb2eebf98deb59bb6218442c2308bb5033ae2915',
                    bios=OWNER/'gbarecomp/bios/gba_bios.bin',config=config,load_state=state,input_replay=trace,
                    strict_static=False,start=start,end=frames[-1],step=15,timeout=180,wide_width=240)
                dest=out/name/label;audit.capture_run(params,dest,'native','composite',frames)
                raw=dest/'raw/native/composite'
                images[label]=[audit.read_png_rgb(raw/f'f_{frame:06d}.png')[2] for frame in frames]
                states[label],findings=audit.load_state_trace(raw/'state.jsonl',frames,label)
                check(not findings,str(findings))
                if label=='on':
                    log=(raw/'stderr.log').read_text(errors='replace')
                    stats=re.findall(r'\[sc3:custom\] matches=(\d+) mismatches=(\d+) incomplete=(\d+)',log)
                    check(stats and int(stats[-1][0])>=60,'Custom renderer did not prove active complete captures')
                    check('native mismatch=' not in log and int(stats[-1][1])==0,'Native replay mismatch')
            check(images['previous']==images['off']==images['on'],name+': pixel mismatch')
            check(states['previous']==states['off']==states['on'],name+': guest state mismatch')
            check(source_hash==hashlib.sha256(state.read_bytes()).hexdigest(),'Source snapshot changed')
            item=dict(case=name,samples=len(frames),native_pixels_identical=True,guest_trace_identical=True,capture_stats=stats[-1])
            report['cases'].append(item);print(item,flush=True)
        report['passed']=True
    except Exception as exc: report['error']=str(exc);raise
    finally: (out/'report.json').write_text(json.dumps(report,indent=2))

if __name__=='__main__': main()
