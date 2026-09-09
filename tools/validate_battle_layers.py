"""Private same-width before/after/rollback and layer replay for battle margins."""
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
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--previous-executable", type=Path, required=True)
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--motion", action="store_true")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    check(output.is_relative_to(ROOT / "validation"), "Use project validation")
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
    trace = runtime / "released.trace"
    trace.write_text('# gbarecomp-keyinput-v1\n0,0x03FF\n')
    os.environ.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                      GBARECOMP_SELFHEAL_RECOMPILE="0", SWORDCRAFT3_BATTLE_MARGIN_MODE="natural",
                      SWORDCRAFT3_BATTLE_HUD_BORDERS="1")
    for key in ["GBARECOMP_VISIBLE_DEBUGGER", "SWORDCRAFT3_ARENA_VIEW", "GBARECOMP_INPUT_RECORD"]:
        os.environ.pop(key, None)
    cap = ROOT / "validation/visible-debugger/20260909-013043-beta/captures"
    cases = [("attack", cap / "frame-0000007550-1788931919405/state.gbas", 7552),
             ("paused", cap / "frame-0000006913-1788931908715/state.gbas", 6915),
             ("village", build / "mods/rom-patches/3f5253fcf57e07ce52472bd29a61d16b98a12376-85d87b74906c486c9b9359a8cf0ec3fd22075394.state5", 11431),
             ("overworld", ROOT / "validation/visible-debugger/20260827-224242-beta/captures/frame-0000033123-1787885173522/state.gbas", 33125)]
    if args.motion:
        cases = [("motion", cap / "frame-0000007550-1788931919405/state.gbas", 7552)]
        trace = cap.parent / "session-input.trace"
    report = {"passed": False, "cases": [], "diagnostic_only": True,
              "sha256": hashlib.sha256((runtime / EXE).read_bytes()).hexdigest()}
    try:
        for name, state, start in cases:
            original_hash = hashlib.sha256(state.read_bytes()).hexdigest()
            for width in ([384] if args.quick else [240,284,384]):
                common = dict(rom=build / "rom-patch-cache/swordcraft3_beta.gba",
                    rom_sha1="bb2eebf98deb59bb6218442c2308bb5033ae2915",
                    bios=ROOT / "gbarecomp/bios/gba_bios.bin", config=config,
                    load_state=state, input_replay=trace, strict_static=False,
                    start=start, end=8152 if args.motion else start+2,
                    step=40 if args.motion else 1, timeout=180, wide_width=width)
                frames = list(range(common["start"],common["end"]+1,common["step"]))
                mode = "native" if width == 240 else "wide"
                def capture(label, layer="composite"):
                    os.environ["SWORDCRAFT3_LEGACY_BATTLE_LAYERS"] = "0" if label == "after" else "1"
                    exe = "previous.exe" if label == "before" else EXE
                    folder = output / name / str(width) / label
                    audit.capture_run(SimpleNamespace(**common, executable=runtime / exe), folder, mode, layer, frames)
                    raw = folder / "raw" / mode / layer
                    imgs = [audit.read_png_rgb(raw / f"f_{f:06d}.png")[2] for f in frames]
                    states, findings = audit.load_state_trace(raw / "state.jsonl", frames, label)
                    check(not findings, str(findings))
                    return imgs, states
                before, old_state = capture("before")
                after, new_state = capture("after")
                rollback, rollback_state = capture("rollback")
                check(old_state == new_state == rollback_state, f"{name}/{width}: guest state changed")
                check(before == rollback, f"{name}/{width}: rollback is not exact")
                left = (width-240)//2
                for a,b in zip(before,after):
                    check(audit.crop_rgb(a,width,left,left+240) == audit.crop_rgb(b,width,left,left+240),
                          f"{name}/{width}: native center changed")
                    if name == "overworld" or width == 240:
                        check(a == b, f"{name}/{width}: protected image changed")
                    else:
                        top = 59 if name == "paused" else 19
                        check(a[:top*width*3] == b[:top*width*3] and a[125*width*3:] == b[125*width*3:],
                              f"{name}/{width}: HUD changed")
                if name == "attack" and width == 384:
                    effect, _ = capture("after", "bg2")
                    for img in effect:
                        for y in range(19,125):
                            check(img[(y*width+312)*3:(y*width+384)*3] == bytes(72*3),
                                  "Duplicate right-hand attack remains")
                    capture("after", "bg0")
                    backdrop, _ = capture("after", "bg1")
                    for img in backdrop:
                        for y in range(19,125):
                            for x in list(range(left))+list(range(left+240,width)):
                                reference = left + ((x-left) % 160)
                                check(img[(y*width+x)*3:(y*width+x+1)*3] ==
                                      img[(y*width+reference)*3:(y*width+reference+1)*3],
                                      "Forest backdrop seam/phase mismatch")
                    capture("after", "obj")
                report["cases"].append(dict(name=name,width=width,native_center_identical=True,
                    guest_state_identical=True,exact_rollback=True,
                    changed_pixels=sum(audit.changed_pixel_count(a,b) for a,b in zip(before,after))))
                print(f"{name}/{width}: PASS", flush=True)
            check(original_hash == hashlib.sha256(state.read_bytes()).hexdigest(), "Input state changed")
        report["passed"] = True
    except Exception as exc:
        report["error"] = str(exc)
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report,indent=2))


if __name__ == "__main__":
    main()
