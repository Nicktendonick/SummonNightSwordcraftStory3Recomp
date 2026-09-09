"""Private replay regression for the September 8 HUD/gauge/edge captures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import audit_widescreen_route as audit

ROOT = Path(__file__).resolve().parents[1]
EXE = "SummonNightSwordcraftStory3RecompBeta.exe"


def check(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--previous-executable", required=True, type=Path)
    parser.add_argument("--pause", action="store_true")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    check(output.is_relative_to(ROOT / "validation"), "Use the project validation folder")
    output.mkdir(parents=True, exist_ok=False)
    runtime = output / "runtime"
    runtime.mkdir()
    build = ROOT / "build-beta"
    for name in [EXE, "SDL2.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"]:
        shutil.copyfile(build / name, runtime / name)
    shutil.copyfile(args.previous_executable, runtime / "previous.exe")
    shutil.copytree(build / "assets", runtime / "assets")
    config = runtime / "game.toml"
    config.write_text('[save]\ntype="eeprom"\nsize="0x2000"\n')
    os.environ.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                      GBARECOMP_SELFHEAL_RECOMPILE="0", SWORDCRAFT3_BATTLE_MARGIN_MODE="natural",
                      SWORDCRAFT3_BATTLE_HUD_BORDERS="1")
    for key in ["SWORDCRAFT3_ARENA_VIEW", "GBARECOMP_VISIBLE_DEBUGGER", "GBARECOMP_INPUT_RECORD",
                "GBARECOMP_AUDIO_DUMP", "GBARECOMP_FRAME_PHASE"]:
        os.environ.pop(key, None)
    captures = ROOT / "validation/visible-debugger/20260908-204353-beta/captures"
    cases = [("left-effect", captures / "frame-0000007020-1788914702737/state.gbas", 7022),
             ("right-gauge", captures / "frame-0000007960-1788914718500/state.gbas", 7962)]
    if args.pause:
        cases = [("pause", cases[1][1], 7990)]
    results = []
    report = {"passed": False, "cases": results, "diagnostic_only": True}
    try:
        for name, state, start in cases:
            original_hash = hashlib.sha256(state.read_bytes()).hexdigest()
            trace = output / f"{name}.trace"
            trace.write_text("# gbarecomp-keyinput-v1\n0,0x03FF\n" +
                             ("7962,0x03F7\n7964,0x03FF\n" if args.pause else ""))
            for width in [285, 384]:
                images, states = {}, {}
                for label, exe in [("before", "previous.exe"), ("after", EXE)]:
                    run = output / name / str(width) / label
                    common = SimpleNamespace(executable=runtime / exe,
                        rom=build / "rom-patch-cache/swordcraft3_beta.gba",
                        rom_sha1="bb2eebf98deb59bb6218442c2308bb5033ae2915",
                        bios=ROOT / "gbarecomp/bios/gba_bios.bin", config=config,
                        load_state=state, input_replay=trace, strict_static=False,
                        start=start, end=start+2, step=1, timeout=90, wide_width=width)
                    frames = list(range(start, start+3))
                    audit.capture_run(common, run, "wide", "composite", frames)
                    raw = run / "raw/wide/composite"
                    images[label] = [audit.read_png_rgb(raw / f"f_{f:06d}.png")[2] for f in frames]
                    states[label], findings = audit.load_state_trace(raw / "state.jsonl", frames, label)
                    check(not findings, str(findings))
                check(states["before"] == states["after"], f"{name}/{width}: guest state changed")
                left = (width-240)//2
                changed_rows = set()
                for before, after in zip(images["before"], images["after"]):
                    check(audit.crop_rgb(before, width, left, left+240) ==
                          audit.crop_rgb(after, width, left, left+240), "Native center changed")
                    for y in range(160):
                        if before[y*width*3:(y+1)*width*3] != after[y*width*3:(y+1)*width*3]:
                            changed_rows.add(y)
                    for y in range(128, 160):
                        # Independent oracle: this capture's blank bottom row,
                        # not the new implementation's per-row sample columns.
                        tan = before[(159*width+left+8)*3:(159*width+left+9)*3]
                        for begin, end in [(0,left),(left+240,width)]:
                            check(after[(y*width+begin)*3:(y*width+end)*3] == tan*(end-begin),
                                  f"{name}/{width}: lower HUD stripe on row {y}")
                    if args.pause:
                        for y in range(59):
                            color = before[(y*width+left+8)*3:(y*width+left+9)*3]
                            for begin, end in [(0,left),(left+240,width)]:
                                check(after[(y*width+begin)*3:(y*width+end)*3] == color*(end-begin),
                                      f"{name}/{width}: repeated paused HUD at row {y}")
                if args.pause:
                    check(changed_rows <= (set(range(59)) | set(range(124,160))),
                          f"Paused gameplay changed: {changed_rows}")
                else:
                    check(changed_rows <= ({18,124} | set(range(125,160))),
                          f"Unexpected changed rows: {changed_rows}")
                results.append(dict(name=name, width=width, native_center_identical=True,
                                    guest_state_identical=True, changed_rows=sorted(changed_rows)))
                print(f"{name}/{width}: PASS, changed rows {sorted(changed_rows)}", flush=True)
            check(hashlib.sha256(state.read_bytes()).hexdigest() == original_hash, "Input state changed")
        report["passed"] = True
    except Exception as exc:
        report["error"] = str(exc)
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
