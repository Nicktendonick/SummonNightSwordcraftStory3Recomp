"""Private owner-capture checks for separate host surface and lake reconstruction."""
import argparse, hashlib, json, os, re, sys
from pathlib import Path
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1]
OWNER=ROOT.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import audit_widescreen_route as audit

def check(ok,message):
    if not ok: raise RuntimeError(message)

def crop(pixels,width):
    left=(width-240)//2
    return b''.join(pixels[(y*width+left)*3:(y*width+left+240)*3] for y in range(160))

def margins(pixels,width):
    left=(width-240)//2
    return b''.join(pixels[y*width*3:(y*width+left)*3]+pixels[(y*width+left+240)*3:(y+1)*width*3] for y in range(160))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--cases',nargs='+',help='Run only these named scene fixtures')
    args=parser.parse_args();out=args.output_dir.resolve()
    check(out.is_relative_to(ROOT/'validation'),'Use experimental validation')
    out.mkdir(parents=True,exist_ok=False)
    runtime=out/'runtime';runtime.mkdir()
    # Reuse the lab binary in place to avoid another 230 MB diagnostic copy.
    # Do not rebuild while this comparison is running.
    executable=ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'
    binary_hash=hashlib.file_digest(executable.open('rb'),'sha256').hexdigest()
    config=runtime/'game.toml';config.write_text('[save]\ntype="eeprom"\nsize="0x2000"\n')
    os.environ.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',GBARECOMP_SELFHEAL_RECOMPILE='0',SWORDCRAFT3_CUSTOM_AUDIT='1')
    for key in ('GBARECOMP_VISIBLE_DEBUGGER','GBARECOMP_INPUT_RECORD','GBARECOMP_VIEW_WIDTH',
                'GBARECOMP_WIDESCREEN','GBARECOMP_RESIZE_VIEW','SWORDCRAFT3_LAKE_EDGE_DATA','SWORDCRAFT3_LAKE_CAMERA_LIMITS'):
        os.environ.pop(key,None)
    c=OWNER/'validation/visible-debugger'
    village=ROOT/'validation/playtest-20260910-232912-511'
    cases=[('lake',c/'20260827-130539-beta/captures/frame-0000019663-1787850529742/state.gbas',19665,True,False),
           ('lake_walk',c/'20260827-130539-beta/captures/frame-0000019663-1787850529742/state.gbas',19665,True,True),
           ('dialogue_entry',c/'20260909-210502-016-beta/captures/frame-0000022877-1789002634502/state.gbas',22879,False,False),
           ('battle',c/'20260827-130539-beta/captures/frame-0000004980-1787850405335/state.gbas',4982,False,False),
           ('critical',c/'20260909-102638-718-beta/captures/frame-0000009933-1788964117707/state.gbas',9935,False,False),
           ('dialogue',c/'20260909-202234-658-beta/captures/frame-0000005967-1788999820512/state.gbas',5969,False,False),
           ('chief',village/'frame-0000016024-1789097370818/state.gbas',16026,True,False),
           ('chief_walk',village/'frame-0000016024-1789097370818/state.gbas',16026,True,True),
           ('chief_approach',village/'frame-0000016024-1789097370818/state.gbas',16026,True,True),
           ('chief_dialogue',village/'frame-0000016799-1789097383972/state.gbas',16801,False,False),
           ('village',village/'frame-0000018812-1789097418343/state.gbas',18814,True,False),
           ('village_walk',village/'frame-0000018812-1789097418343/state.gbas',18814,True,True)]
    if args.cases:
        check(set(args.cases)<={c[0] for c in cases},'Unknown scene fixture')
        cases=[c for c in cases if c[0] in args.cases]
    report=dict(passed=False,executable_sha256=binary_hash,cases=[],limitations=['Three authenticated field scenes only; no omitted sprites, battle extension, dialogue overlay or adaptive resize.'])
    try:
        for name,state,start,expect_lake,walk in cases:
            source_hash=hashlib.sha256(state.read_bytes()).hexdigest()
            frames=list(range(start,start+91));baseline=None;baseline_states=None
            trace=runtime/(name+'.trace')
            # Approaching the chief to the right starts his automatic greeting.
            # Short upward walks exercise free movement away from that trigger
            # (and away from the adjoining village's NPC conversation trigger).
            safe_walk=name in ('chief_walk','village_walk')
            walk_key='0x03BF' if safe_walk else '0x03EF'
            walk_frames=24 if safe_walk else 60
            trace.write_text('# gbarecomp-keyinput-v1\n0,0x03FF\n'+
                             (f'{start},{walk_key}\n{start+walk_frames},0x03FF\n' if walk else ''))
            for width in (240,284,320,384):
                os.environ['SWORDCRAFT3_CUSTOM_RENDERER']='1' if width>240 else '0'
                os.environ['SWORDCRAFT3_CUSTOM_HOST_WIDTH']=str(width)
                params=SimpleNamespace(executable=executable,
                    rom=OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba',rom_sha1='bb2eebf98deb59bb6218442c2308bb5033ae2915',
                    bios=OWNER/'gbarecomp/bios/gba_bios.bin',config=config,load_state=state,input_replay=trace,
                    strict_static=False,start=start,end=frames[-1],step=1,timeout=180,wide_width=240)
                dest=out/name/str(width);audit.capture_run(params,dest,'native','composite',frames)
                raw=dest/'raw/native/composite'
                images=[];colored=0;coverage=[]
                for frame in frames:
                    w,h,pixels=audit.read_png_rgb(raw/f'f_{frame:06d}.png')
                    check((w,h)==(width,160),f'{name}/{width}: unexpected output {w}x{h}')
                    images.append(crop(pixels,w));margin=margins(pixels,w)
                    colored+=sum(v!=0 for v in margin);coverage.append(any(margin))
                # Host-only widening must preserve PPU and aggregate state too,
                # not merely the reduced trace fields used by old wide tests.
                states={row['frame']:row for row in map(json.loads,(raw/'state.jsonl').read_text().splitlines())
                        if start<=row['frame']<=frames[-1]}
                check(len(states)>=len(frames),'Missing guest state records')
                if width==240: baseline=images;baseline_states=states
                else:
                    check(images==baseline,f'{name}/{width}: changed native pixels')
                    check(states==baseline_states,f'{name}/{width}: changed full guest state trace')
                    log=(raw/'stderr.log').read_text(errors='replace')
                    stats=re.findall(r'\[sc3:host\] width=(\d+) lake=(\d+) fallback=(\d+) guest=240',log)
                    check(stats,'Missing host renderer diagnostic')
                    check('native mismatch=' not in log,'Native replay mismatch')
                    check((colored>0)==expect_lake,f'{name}/{width}: unexpected margin coverage ({colored})')
                    check((int(stats[-1][1])>0)==expect_lake,f'{name}/{width}: wrong scene authorization')
                    if expect_lake:
                        declines=re.findall(r'\[sc3:host-decline\] completed=\d+ reason=(\S+)',log)
                        permitted={'lake-player-control'} if name=='chief_approach' else set()
                        check(set(declines)<=permitted,f'{name}/{width}: unexpected intermittent fallback {set(declines)}')
                        if name=='chief_approach':
                            check(coverage[0] and not coverage[-1] and
                                  sum(a!=b for a,b in zip(coverage,coverage[1:]))==1,
                                  'Chief greeting did not switch cleanly from wide to native')
                        else: check(all(coverage),f'{name}/{width}: missing wide frame')
                    item=dict(case=name,width=width,sampled_frames=len(frames),full_state_records=len(states),
                              native_pixels_identical=True,full_guest_state_identical=True,
                              nonzero_margin_bytes=colored,stats=stats[-1])
                    report['cases'].append(item);print(item,flush=True)
            check(source_hash==hashlib.sha256(state.read_bytes()).hexdigest(),'Source snapshot changed')
        check(binary_hash==hashlib.file_digest(executable.open('rb'),'sha256').hexdigest(),'Executable changed during validation')
        report['passed']=True
    except Exception as exc: report['error']=str(exc);raise
    finally: (out/'report.json').write_text(json.dumps(report,indent=2))

if __name__=='__main__': main()
