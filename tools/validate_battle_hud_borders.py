"""Replay private captures to validate game-owned battle HUD borders on/off.

All executables, generated state/logs and images are isolated beneath the
project's ignored validation folder. Never ships or modifies private inputs.
"""
from __future__ import annotations

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
ROM_STEM = "3f5253fcf57e07ce52472bd29a61d16b98a12376-85d87b74906c486c9b9359a8cf0ec3fd22075394"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def compare(condition, description):
    if not condition:
        raise RuntimeError(description)


def same_except_raster_seams(before, after, width):
    """Only the corrected BG0 margin rows 18/124 may differ from capstone."""
    left = (width - 240) // 2
    return all(
        (audit.crop_rgb(a, width, left, left+240) == audit.crop_rgb(b, width, left, left+240)) and
        all(a[y*width*3:(y+1)*width*3] == b[y*width*3:(y+1)*width*3]
            for y in range(160) if y not in (18,124))
        for a, b in zip(before.values(), after.values()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-beta")
    parser.add_argument("--baseline-executable", type=Path, default=ROOT /
        "release-stage/alpha-unfinished-build-v.01/20260908T001923Z-1e08648b/payload" / EXE)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    compare(output.is_relative_to((ROOT / "validation").resolve()) and output != ROOT / "validation",
            "Output must be a new directory underneath this project's validation folder")
    output.mkdir(parents=True, exist_ok=False)
    runtime = output / "isolated-runtime"
    runtime.mkdir()
    for name in [EXE, "SDL2.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"]:
        shutil.copyfile(args.build_dir / name, runtime / name)
    baseline_hash = digest(args.baseline_executable)
    compare(baseline_hash in {
        "0bafb1fc1e24ea41670dffb423e8274c1a8a25438787710ad6521641c176a190",
        "6928d8afda7a24d7e0a23d0250c0d559d2360f471a7951f0c2d710683be39cfb",
    }, "Baseline must be the original capstone executable or its verified stripped copy")
    baseline_exe = runtime / "capstone-baseline.exe"
    shutil.copyfile(args.baseline_executable, baseline_exe)
    for relative in [
        "fonts/LatoLatin-Regular.ttf", "fonts/LatoLatin-Bold.ttf",
        "fonts/OpenMoji-black-glyf.ttf", "fonts/NotoSansSymbols2-Regular.ttf",
        "img/brand_mark.tga", "img/pad_gba.tga", "img/verdict_ok.tga",
        "img/verdict_bad.tga", "img/verdict_warn.tga", "img/verdict_none.tga",
    ]:
        target = runtime / "assets" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.build_dir / "assets" / relative, target)
    config = runtime / "game.toml"
    config.write_text('[save]\ntype = "eeprom"\nsize = "0x2000"\n', encoding="utf-8")

    os.environ.update({
        "SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy",
        "GBARECOMP_SELFHEAL_RECOMPILE": "0", "SWORDCRAFT3_BATTLE_MARGIN_MODE": "natural",
    })
    for key in ["SWORDCRAFT3_ARENA_VIEW", "GBARECOMP_INPUT_REPLAY", "GBARECOMP_INPUT_RECORD",
                "GBARECOMP_AUDIO_DUMP", "GBARECOMP_FRAME_PHASE"]:
        os.environ.pop(key, None)
    captures = ROOT / "validation/visible-debugger/20260827-130539-beta/captures"
    cases = [
        ("village", args.build_dir / "mods/rom-patches" / f"{ROM_STEM}.state5", 11431, True),
        ("forest", captures / "frame-0000004980-1787850405335/state.gbas", 4982, True),
        ("overworld", ROOT / "validation/visible-debugger/20260827-224242-beta/captures/frame-0000033123-1787885173522/state.gbas", 33125, False),
    ]
    report = {"executable_sha256": digest(runtime / EXE), "baseline_sha256": baseline_hash, "cases": [],
              "interpretation": "Diagnostic replay, not fully-static or all-game certification",
              "live_resize_and_menu_playtest": "Manual check still required"}
    try:
        for name, state, start, battle in cases:
            print(f"Replaying {name}...", flush=True)
            state_hash = digest(state)
            frames = list(range(start, start + 5))
            common = dict(executable=runtime / EXE,
                          rom=args.build_dir / "mods/rom-patches" / f"{ROM_STEM}.gba",
                          rom_sha1="bb2eebf98deb59bb6218442c2308bb5033ae2915",
                          bios=ROOT / "gbarecomp/bios/gba_bios.bin", config=config,
                          load_state=state, input_replay=None, strict_static=False,
                          start=start, end=start + 4, step=1, timeout=90)
            result = {"name": name, "state_sha256": state_hash, "frames": frames,
                      "widths": {}, "coverage": {}}

            def capture(width, on, layer="composite", baseline=False):
                label = "capstone" if baseline else "on" if on else "off"
                folder = output / name / f"{width}-{label}"
                mode = "native" if width == 240 else "wide"
                run_args = SimpleNamespace(**common, wide_width=width)
                if baseline:
                    run_args.executable = baseline_exe
                os.environ["SWORDCRAFT3_BATTLE_HUD_BORDERS"] = "1" if on else "0"
                audit.capture_run(run_args, folder, mode, layer, frames)
                raw = folder / "raw" / mode / layer
                images = {}
                for frame in frames:
                    w, h, data = audit.read_png_rgb(raw / f"f_{frame:06d}.png")
                    compare(w == width and h == 160, f"Unexpected dimensions in {raw}")
                    images[frame] = data
                trace, findings = audit.load_state_trace(raw / "state.jsonl", frames, str(raw))
                compare(not findings, f"Missing state evidence in {raw}: {findings}")
                result["coverage"][f"{width}-{label}-{layer}"] = json.loads((raw / "coverage.json").read_text())
                return images, trace, raw

            native_off, native_state, _ = capture(240, False)
            native_on, native_on_state, _ = capture(240, True)
            compare(native_off == native_on and native_state == native_on_state,
                    f"{name}: Native mode changed with HUD toggle")
            old_native, old_native_state, _ = capture(240, False, baseline=True)
            compare(old_native == native_off and old_native_state == native_state,
                    f"{name}: Native differs from the capstone")
            for width in ([284, 320, 384] if battle else [384]):
                off, off_state, off_raw = capture(width, False)
                on, on_state, on_raw = capture(width, True)
                policies = [json.loads(line[len(audit.TRACE_PREFIX):])["policy"]
                            for line in (on_raw / "stderr.log").read_text().splitlines()
                            if line.startswith(audit.TRACE_PREFIX)]
                expected_family = "battle_" if battle else "field_"
                compare(policies and all(policy.startswith(expected_family) for policy in policies),
                        f"{name}: capture does not identify the expected {expected_family} scene")
                compare(on_state == off_state, f"{name}/{width}: HUD toggle changed guest state")
                old_wide, old_wide_state, _ = capture(width, False, baseline=True)
                compare(same_except_raster_seams(old_wide, off, width) and old_wide_state == off_state,
                        f"{name}/{width}: Off mode differs from the capstone outside corrected raster seams")
                # Widening the existing game-owned OBJ culling can change guest
                # execution/OAM relative to Native. Do NOT call that equal or
                # blame this final-frame feature: prove exact old/new equality
                # at each width, and retain the original cross-width audit gap.
                native_state_equal = off_state == native_state
                changed = 0
                left = (width - 240) // 2
                for frame in frames:
                    a, b = off[frame], on[frame]
                    compare(audit.crop_rgb(b, width, left, left + 240) == native_off[frame],
                            f"{name}/{width}/{frame}: Native center changed")
                    if not battle:
                        compare(a == b, f"{name}: nonbattle image changed")
                        continue
                    compare(a[19 * width * 3:125 * width * 3] == b[19 * width * 3:125 * width * 3],
                            f"{name}/{width}/{frame}: gameplay band changed")
                    for y in list(range(19)) + list(range(125, 160)):
                        sample = left + (176 if 128 <= y < 144 else 225 if y >= 144 else 8)
                        color = a[(y * width + sample) * 3:(y * width + sample + 1) * 3]
                        for begin, end in [(0, left), (left + 240, width)]:
                            compare(b[(y * width + begin) * 3:(y * width + end) * 3] == color * (end - begin),
                                    f"{name}/{width}/{frame}: HUD band did not match live color at y={y}")
                    changed += audit.changed_pixel_count(a, b)
                compare(not battle or changed > 0, f"{name}/{width}: borders never applied")
                result["widths"][str(width)] = {"passed": True, "changed_margin_pixels_across_frames": changed,
                    "native_center_identical": True, "hud_toggle_guest_state_identical": True,
                    "capstone_pixels_identical_except_margin_rows_18_124": True,
                    "capstone_guest_state_identical": True,
                    "native_wide_guest_state_identical": native_state_equal,
                    "preexisting_native_wide_state_difference": not native_state_equal,
                    "gameplay_band_unchanged": True,
                    "on_frame": str((on_raw / f"f_{start:06d}.png").relative_to(output)),
                    "off_frame": str((off_raw / f"f_{start:06d}.png").relative_to(output))}
                # Compare rollback against the exact preserved capstone capture
                # only when the saved-state identity is still the original one.
                if name == "village" and width == 384 and state_hash == "40183a9bd6c51e5eb64f86e6eeb44c9eaca4e23706e698f336d46ec12f3d1038":
                    baseline = ROOT / "validation/adaptive-widescreen/battle-state5-natural-final-384-20260831/raw/wide/composite"
                    for frame in frames:
                        _, _, old = audit.read_png_rgb(baseline / f"f_{frame:06d}.png")
                        compare(same_except_raster_seams({frame: old}, {frame: off[frame]}, width),
                                "Off mode differs from preserved capstone outside raster seams")
                    result["capstone_pixel_identical_except_raster_seams"] = True
            if battle:
                for layer in ("bg0", "bg1", "bg2", "obj"):
                    off, off_state, _ = capture(384, False, layer)
                    on, on_state, _ = capture(384, True, layer)
                    compare(off == on and off_state == on_state, f"{name}: {layer} isolation contaminated by borders")
                result["isolated_layers_unchanged"] = ["bg0", "bg1", "bg2", "obj"]
            compare(digest(state) == state_hash, "Input save state was modified")
            result["passed"] = True
            report["cases"].append(result)
            print(f"{name}: PASS", flush=True)
        report["passed"] = True
    except Exception as exc:
        report["passed"] = False
        report["error"] = str(exc)
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"All replay checks passed: {output / 'report.json'}")


if __name__ == "__main__":
    main()
