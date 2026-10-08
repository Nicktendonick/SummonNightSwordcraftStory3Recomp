# Launcher and Esc cleanup — first test pass

User-facing date: October 3, 2026. Validation directory names use UTC.

## Try it

Run `release/Portable UI Test/Swordcraft Story 3 Beta.exe` under the owner
project. This is a separate private copy, not a replacement or public release.
It contains private copies of ROM/BIOS, settings and saves; do not redistribute
this played test directory.

Both Portable Beta and Portable Camera Edge Test were hash-checked unchanged.
The final copy received the original settings/saves, not the settings exercised
by automation.

## Layout

- Home: artwork and game/save summary. Full file paths, BIOS and the existing
  confirmed Restore launcher defaults action live under Game files.
- Graphics: Window & Screen, Picture, Battle View. Presets and individual filters
  remain available. Camera framing is one choice, not two competing switches.
- Audio: volume, with sample rate under Advanced audio. Zero volume mutes.
- Controls: gameplay bindings and the existing launcher hotkeys.
- Assist Tools: retains enable, speed and assist bindings.
- Mods: Hold Select to Guard and **English Translation** remain here.
- Credits: existing original/port/tools content.
- Esc: Display joins Graphics, using the same category names and graphics
  labels. Pause/Resume remain paired; Reset/Quit are grouped under End session
  and still require confirmation. The permanent Resume footer invokes the
  Resume action rather than hiding the menu while leaving a manual pause active.

The parchment/plum/teal theme is retained. The launcher header is shorter.
Cover scenery edges is unavailable unless Follow + edge stops is selected.
Disabling that control does not discard its remembered value.

## Scope and boundaries

No game input, combat camera math, renderer algorithms or translation data
were changed for this pass. Existing settings callbacks and preference files
are reused. The organized layout is opt-in at the shared launcher/runtime
boundary; legacy hosts keep their previous layout and footer semantics.

Controls and Mods inside Esc are deferred. Launcher sample rate and file
selection remain pre-launch operations. Runtime Sound and FPS are still
runtime-specific controls. No GitHub push, release upload, or main-portable
promotion was performed.

## Verification

- Both English 1.0.6.f and Japanese engines rebuilt.
- Full configured CTest suite: **58/58 passed**. The first broad run discovered
  17 unbuilt test programs; they were built and the complete suite rerun.
- Launcher model tests cover opt-in view routing, all camera values, invalid
  values, unavailable controls and rejected writes without unrelated changes.
- Runtime tests cover grouping/order and layout bounds at 1280x720, 960x640
  and 640x480. A simulated click on the measured footer row invokes Resume.
- Real packaged launcher: all seven pages rendered; camera modes changed and
  persisted; Select-to-Guard toggled both ways; translation toggled both ways.
- Both real language launch paths passed the Esc/manual-pause/auto-pause,
  resume, cancel-reset and cancel-quit input checks, with zero dispatch misses.
- Engine stripping preserved loaded sections, imports and entry points.
  Packaged dependency checks passed.

Evidence:
`validation/ui-cleanup-20261004-032911-ca29/report.json` and adjacent logs;
`build-native/Testing/Temporary/LastTest.log`.

UI checks use model, callback and layout state, not screenshot matching or
pixel-based scene detection. This is not an exhaustive gameplay or aesthetic
review. The next step is the owner's layout feedback on the separate test.

The repeatable smoke/preparation tool is `tools/validate_ui_cleanup.py`.
It refuses to overwrite an existing Portable UI Test.

## Header wording — October 6, 2026

The launcher and Esc menu share the owner's exact revised wording, with a
hard line break after `weapons`:

```text
Craftnights! Artisans of weapons
Masters of the sword!
```

This supersedes the earlier single-line quote. No gameplay or saved-setting
changes are involved. The October 3 tester ZIP is a separate, unchanged snapshot.

Both language engines were rebuilt and installed in Portable UI Test. The two
targeted launcher/runtime-menu tests passed, including Esc layout at three sizes
with the revised subtitle. Both delivered binaries contain the exact newline;
stripping preserved loaded sections, imports and entry points. Portable preflight
passed, and all 79 other installed files were hash-checked unchanged.
Previous engines are retained in
`validation/tagline-20261006-212847/rollback` under this checkout.

## Corrected header wording — October 7, 2026

The owner corrected the spelling and requested three separate lines, each ending
in an exclamation mark. The shared launcher/Esc subtitle now reads:

```text
Craftknights!
Artisans of weapons!
Masters of the sword!
```

This replaces the October 6 wording. The existing tester ZIP remains unchanged.

Both language engines were rebuilt and installed in Portable UI Test. Both
targeted UI tests passed, including the three-size Esc layout test with this
subtitle. The stripped programs retain identical loaded code to their build
outputs and contain the exact three-line string. Portable preflight passed;
all 80 other installed files were hash-checked unchanged. Rollback programs:
`validation/tagline-20261007-012628/rollback` under this checkout.
