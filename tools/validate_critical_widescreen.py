"""Private same-width critical-hit before/after/rollback snapshot regression."""
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
    parser.add_argument("--previous-executable", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
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
                      SWORDCRAFT3_BATTLE_HUD_BORDERS="1", SWORDCRAFT3_LEGACY_BATTLE_LAYERS="0")
    for key in ["GBARECOMP_VISIBLE_DEBUGGER", "GBARECOMP_INPUT_RECORD", "GBARECOMP_VIEW_WIDTH",
                "GBARECOMP_WIDESCREEN", "GBARECOMP_RESIZE_VIEW", "SWORDCRAFT3_ARENA_VIEW"]:
        os.environ.pop(key, None)
    captures = ROOT / "validation/visible-debugger/20260909-102638-718-beta/captures"
    cases = [
        ("critical", captures / "frame-0000009933-1788964117707/state.gbas", 9935, 10085, 5),
        ("rocky", captures / "frame-0000061510-1788964715395/state.gbas", 61512, 61518, 3),
        ("overworld", ROOT / "validation/visible-debugger/20260827-224242-beta/captures/frame-0000033123-1787885173522/state.gbas", 33125, 33127, 1),
    ]
    report = dict(passed=False, cases=[], sha256=hashlib.sha256((runtime / EXE).read_bytes()).hexdigest(),
                  limitations=["Released-control snapshot continuation, not replay of the discontinuous save/load input trace.",
                               "Same-width parity; does not certify pre-existing native-vs-wide equivalence."])
    try:
        for name, state, start, end, step in cases:
            original_hash = hashlib.sha256(state.read_bytes()).hexdigest()
            for width in ([240, 284, 384] if name == "critical" else [384]):
                frames = list(range(start, end + 1, step))
                mode = "native" if width == 240 else "wide"
                def capture(label, layer="composite"):
                    os.environ["SWORDCRAFT3_CRITICAL_WIDESCREEN"] = "0" if label == "rollback" else "1"
                    folder = output / name / str(width) / label
                    params = SimpleNamespace(executable=runtime / ("previous.exe" if label == "before" else EXE),
                        rom=build / "rom-patch-cache/swordcraft3_beta.gba",
                        rom_sha1="bb2eebf98deb59bb6218442c2308bb5033ae2915",
                        bios=ROOT / "gbarecomp/bios/gba_bios.bin", config=config,
                        load_state=state, input_replay=trace, strict_static=False,
                        start=start, end=end, step=step, timeout=180, wide_width=width)
                    audit.capture_run(params, folder, mode, layer, frames)
                    raw = folder / "raw" / mode / layer
                    images = [audit.read_png_rgb(raw / f"f_{f:06d}.png")[2] for f in frames]
                    states, findings = audit.load_state_trace(raw / "state.jsonl", frames, label)
                    check(not findings, str(findings))
                    return images, states, raw
                before, old_state, _ = capture("before")
                after, new_state, raw = capture("after")
                rollback, rollback_state, _ = capture("rollback")
                check(old_state == new_state == rollback_state, f"{name}/{width}: guest state changed")
                check(before == rollback, f"{name}/{width}: rollback not exact")
                left = (width - 240) // 2
                for a, b in zip(before, after):
                    check(audit.crop_rgb(a, width, left, left + 240) == audit.crop_rgb(b, width, left, left + 240),
                          f"{name}/{width}: original center changed")
                if name != "critical" or width == 240:
                    check(before == after, f"{name}/{width}: protected picture changed")
                else:
                    telemetry, findings = audit.parse_trace(raw / "stderr.log", frames)
                    check(not findings, str(findings))
                    critical = [i for i, row in enumerate(telemetry) if row["policy"] == "battle_critical"]
                    check(critical, "Critical layout not exercised")
                    for i in critical:
                        b = after[i]
                        for y, native_x in [(7, 8), (145, 225)]:
                            wanted = b[(y*width+left+native_x)*3:(y*width+left+native_x+1)*3]
                            for x in list(range(left)) + list(range(left+240, width)):
                                check(b[(y*width+x)*3:(y*width+x+1)*3] == wanted, "Critical HUD wing missing")
                    check(any(a != b for a,b in zip(before,after)), "No critical extension rendered")
                    if width == 384:
                        for layer in ("bg0", "bg1", "bg2", "obj"):
                            capture("after", layer)
                report["cases"].append(dict(name=name, width=width, samples=len(frames),
                    native_center_identical=True, guest_state_identical=True, exact_rollback=True,
                    changed_pixels=sum(audit.changed_pixel_count(a,b) for a,b in zip(before,after))))
                print(f"{name}/{width}: PASS", flush=True)
            check(original_hash == hashlib.sha256(state.read_bytes()).hexdigest(), "Source snapshot changed")
        report["passed"] = True
    except Exception as exc:
        report["error"] = str(exc)
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
