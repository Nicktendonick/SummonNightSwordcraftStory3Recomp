# Graphics presets — 2026-09-29

## Contract

Add the same five beginner-friendly filter recipes to launcher Settings > Display
and in-game Esc > Graphics. No guest-memory changes, game hooks, camera/viewport
changes, audio changes, or shader/filter algorithm changes are part of this work.
The existing renderer scene coverage and native fallback are unchanged.

| Name | Scaling | Screen effect | Strength |
| --- | --- | --- | --- |
| Original Pixels | Nearest | Off | 35 (inactive) |
| Clean & Crisp | Sharp fractional | Off | 35 (inactive) |
| Soft & Smooth | Smooth 2x | Off | 35 (inactive) |
| Handheld Grid | Nearest | LCD Grid | 25 |
| Retro TV | Linear | CRT | 35 |

Clean & Crisp is the suggested starting point, not a forced new default.
The current installation's settings are retained. Presets are visual styles,
not fidelity/performance tiers, and deliberately leave the colour model, aspect
ratio, window size/fullscreen, controls, sound, saves and Guard preference alone.
The screen model remains separately adjustable; the preset names do not claim
exact emulation of a physical GBA display.

## Ownership and persistence

- Game-owned recipes and explanations: src/graphics_presets.h, passed by main.cpp.
- Generic descriptor/recognition: recomp-ui/src/recomp_graphics_preset.h.
- Launcher model and ImGui combo: recomp-ui/src/common/launcher_model.c and
  common/backends/imgui/launcher_imgui.cpp.
- Generic live menu: gbarecomp/src/runtime/runtime.cpp.
- Optional per-choice descriptions/unknown-choice label: RecompRuntimeUiItem.
  Unknown Custom is display-only; forward navigation starts at the first recipe,
  backward at the last. Existing sparse choice values remain supported.

No preset ID or additional configuration file is saved. Both interfaces infer
the name from the actual scaler/effect/strength tuple, avoiding stale labels after
manual changes. Inactive strength is ignored for effects Off. A nonmatching
combination displays Custom; selecting a preset again reapplies its full recipe.
The live handler atomically saves the existing five launcher.ini filter keys
before changing HostWindow. Rejected indices and failed writes change nothing.
Reset Game already refreshes these five keys, so no reset protocol changes were
necessary.

## Verification

Exact staged payload and isolated evidence:
validation/graphics-presets-20260929-233958-8fe0/

- Both English and Japanese release engines built successfully.
- 20 targeted CTest checks passed after rebuilding affected test executables;
  five portable-package Python tests also passed.
- Launcher unit tests cover every recipe, capability gates, invalid indices,
  inferred Custom, inactive strength, Restore Defaults and byte-for-byte
  preservation of unrelated settings fields.
- Runtime UI tests cover Custom navigation in both directions, wrapping, sparse
  values, dynamic descriptions and rejected writes.
- Actual normal portable starter exercised every recipe with only system DLL
  search paths and no user ROM/BIOS/save inputs.
- Live English combat fixture and Japanese cold-boot session exercised all five
  recipes, manual Custom, invalid index, Pause/Resume, Reset Game, launcher
  reload/edit/reopen and an intentionally read-only settings file.
- Guest frame number, CPU cycle count and PC remained identical throughout
  paused preset changes. Resume advanced execution. Reset retained Handheld Grid.
- Read-only persistence failure retained Clean & Crisp and identical INI bytes.
- Private original ROMs, BIOS and combat state hashes remained unchanged.

Tests use state/control-flow/settings assertions, not screenshots or framebuffer
comparisons. This is not an exhaustive playthrough, new performance benchmark,
or visual approval of each look; users may prefer different colour models and
effect strengths. Japanese combat fixture coverage remains unavailable.

## Portable update

tools/update_graphics_presets.py stages stripped copies while verifying identical
loaded PE sections, imports and entry point, checks bundled dependency closure,
then tests the exact staged payload before installation.

Only Runtime/Swordcraft3CustomRendererBeta.exe, Runtime/Swordcraft3Japanese.exe and
README.md are allowlisted for replacement. Installation checks the whole current
folder against its preflight snapshot, saves hash-verified rollback copies, runs
the normal starter's --check, and verifies every other file unchanged. Exact
hashes, backup location and result are recorded in PREPARED.json, SMOKE.json and
INSTALLED.json in the evidence folder.

No source commit, GitHub publication or replacement release ZIP is part of this
update.
