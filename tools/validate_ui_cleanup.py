"""Build-independent, private UI smoke and separate portable preparation.

Uses the existing launcher/model and runtime input scripts, never pixel tests.
Reads the two existing portables; only disposable copies receive test inputs.
No archive, publication, migration or replacement of an existing folder.
"""
import configparser
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from benchmark_presentation_filters import environment
from package_alpha import check_dependencies, file_hash, pe_info
from package_follow_edge_test import execute, require_local_selection
from update_camera_test_f10 import ESC_SCRIPT, verify_escape
from update_guard_performance import ROOT, OWNER, snapshot, stamp

SOURCE = OWNER / "release/Portable Camera Edge Test"
MAIN = OWNER / "release/Portable Beta"
TARGET = OWNER / "release/Portable UI Test"
STARTER = "Swordcraft Story 3 Beta.exe"
ENGINES = {
    "Swordcraft3CustomRendererBeta.exe": ROOT / "build-native/Swordcraft3Translation106.exe",
    "Swordcraft3Japanese.exe": ROOT / "build-native/Swordcraft3Japanese106.exe",
}
COPY_DIRS = ("Runtime", "Credits", "BIOS", "ROMs", "Mods", "Settings", "Saves", "Save States")


def private_copy(destination, engines):
    assert not destination.exists(), "Refusing to overwrite: " + str(destination)
    assert destination.resolve().is_relative_to(OWNER.resolve())
    destination.mkdir()
    for directory in COPY_DIRS:
        shutil.copytree(SOURCE / directory, destination / directory)
    for directory in ("Logs", "Captures"):
        (destination / directory).mkdir()
    shutil.copyfile(SOURCE / STARTER, destination / STARTER)
    shutil.copyfile(SOURCE / "README.md", destination / "README.md")
    for name in ENGINES:
        shutil.copyfile(engines / name, destination / "Runtime" / name)
    require_local_selection(destination)


def main():
    assert not TARGET.exists(), "Portable UI Test already exists; do not overwrite it."
    require_local_selection(SOURCE)
    source_before, main_before = snapshot(SOURCE), snapshot(MAIN)
    inputs = {str(path): file_hash(path) for path in ENGINES.values()}
    stage = ROOT / "validation" / ("ui-cleanup-" + stamp())
    engines = stage / "engines"
    engines.mkdir(parents=True)
    print("STAGE=" + str(stage), flush=True)
    metadata = {name: pe_info(SOURCE / "Runtime" / name) for name in
                ("SDL2.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")}
    metadata[STARTER] = pe_info(SOURCE / STARTER)
    for name, source in ENGINES.items():
        candidate = engines / name
        shutil.copyfile(source, candidate)
        original = pe_info(source)
        subprocess.run(["C:/msys64/mingw64/bin/strip.exe", "--strip-debug", str(candidate)],
                       check=True, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
        stripped = pe_info(candidate)
        assert all(original[k] == stripped[k] for k in ("loaded_sections", "imports", "entry_rva"))
        metadata[name] = stripped
    check_dependencies(metadata)
    test = stage / "smoke"
    private_copy(test, engines)
    subprocess.run([str(test / STARTER), "--check"], cwd=stage, check=True,
                   timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
    neutral = stage / "neutral.trace"
    neutral.write_text("# gbarecomp-keyinput-v1\n0,0x03ff\n", encoding="ascii")
    env = environment(test, 384, neutral, audit=True)
    # The real launcher requires OpenGL. Game input remains exclusive neutral.
    env.pop("SDL_VIDEODRIVER", None)
    env.pop("SDL_RENDER_DRIVER", None)
    env.pop("SWORDCRAFT3_BATTLE_HUD_BORDERS", None)
    system = os.environ.get("SystemRoot", os.environ.get("SYSTEMROOT"))
    env["PATH"] = system + "/System32;" + system
    env["GBARECOMP_STRICT_STATIC"] = "1"
    # No captures needed for a UI-state test.
    env["GBARECOMP_ASSIST_SCRIPT"] = ESC_SCRIPT.replace("20:capture;", "").replace("42:capture;", "")
    cfg = configparser.ConfigParser()
    cfg.read(test / "Settings/battle-camera.ini", encoding="utf-8-sig")
    mode = cfg.getint("Launcher", "battle_camera_mode", fallback=0)
    cover = cfg.getint("Launcher", "battle_edge_cover", fallback=0)
    pages = ("dashboard", "graphics", "audio", "controller", "assist_tools", "mods", "credits")
    navigation = ";".join("view:" + p + ";wait:3;uiprobe" for p in pages)
    env["LNG_SCRIPT"] = (
        "wait:8;" + navigation +
        ";view:graphics;builtinset:1:0;builtinset:2:1;builtinset:1:1;builtinset:1:2;"
        "builtinset:2:0;builtinset:2:1;builtinprobe;"
        f"builtinset:2:{cover};builtinset:1:{mode};"
        "view:mods;builtintoggle:0;builtintoggle:0;romprobe;patchtoggle;romprobe;"
        "patchtoggle;romprobe;play"
    )
    args = ["--window", "--launcher", "--save", str(test / "Saves/ui-smoke.eep"),
            str(test / "Runtime/game.toml")]
    english = execute(test / "Runtime/Swordcraft3CustomRendererBeta.exe", args, stage, env, "english")
    for page in ("Dashboard", "Graphics", "Audio", "Controller", "Assist Tools", "Mods", "Credits"):
        assert "[launcher:ui] organized=1 view=" + page + " " in english, page
    assert "[launcher:builtin-set] index=2 value=1 accepted=0" in english
    for value in (0, 1, 2):
        assert f"[launcher:builtin-set] index=1 value={value} accepted=1" in english
    assert "verified=1 can_play=1 patch=0 language=Japanese" in english
    assert "verified=1 can_play=1 patch=1 language=English (translation 1.0.6.f)" in english
    assert "dispatch_misses=0" in (stage / "english.out").read_text(errors="replace")
    report = {"english": verify_escape(english), "launcher_pages": list(pages),
              "camera_choices_and_dependency": True, "translation_still_in_mods": True}
    # Use the same Mods translation switch to launch the Japanese engine.
    env["LNG_SCRIPT"] = "wait:8;view:mods;patchtoggle;romprobe;play"
    japanese = execute(test / "Runtime/Swordcraft3CustomRendererBeta.exe", args, stage, env, "japanese")
    assert "verified=1 can_play=1 patch=0 language=Japanese" in japanese
    assert "dispatch_misses=0" in (stage / "japanese.out").read_text(errors="replace")
    report["japanese"] = verify_escape(japanese)
    assert snapshot(SOURCE) == source_before, "Existing Camera Edge Test changed externally"
    assert snapshot(MAIN) == main_before, "Main portable changed externally"
    assert all(file_hash(Path(path)) == digest for path, digest in inputs.items()), "Candidate build changed"
    # Final copy comes from the untouched source, not the exercised smoke copy.
    private_copy(TARGET, engines)
    (TARGET / "UI-TEST.txt").write_text(
        "Private UI cleanup test; not a public release.\n\n"
        "Run Swordcraft Story 3 Beta.exe in this folder.\n"
        "Home: game/save summary; expandable Game files.\n"
        "Graphics: Window & Screen, Picture, Battle View.\n"
        "Audio: volume; sample rate under Advanced audio.\n"
        "Controls: gameplay bindings and launcher hotkeys.\n"
        "Mods: Select-to-Guard and English Translation.\n"
        "Esc: matching Graphics groups and persistent Resume.\n\n"
        "Controls and Mods inside Esc are a later pass.\n"
        "This copy has private copies of your inputs/settings/saves; do not redistribute it.\n"
        "Portable Beta and Portable Camera Edge Test were not replaced.\n",
        encoding="utf-8")
    report.update(passed=True, private_only=True, no_pixel_assertions=True,
                  source_portables_unchanged=True, source_engine_hashes=inputs,
                  target=str(TARGET), settings_copied_without_test_changes=True,
                  source_settings={key: value for key, value in source_before.items()
                                   if key.startswith(("Settings/", "Saves/", "Save States/"))},
                  final_engine_hashes={name: file_hash(TARGET / "Runtime" / name) for name in ENGINES})
    assert all(file_hash(TARGET / key) == digest for key, digest in report["source_settings"].items())
    assert snapshot(SOURCE) == source_before and snapshot(MAIN) == main_before
    (stage / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS: both language launch paths, seven launcher pages, camera choices, Esc/pause/resume; existing portables unchanged", flush=True)
    print("LAUNCHER=" + str(TARGET / STARTER), flush=True)


if __name__ == "__main__":
    main()
