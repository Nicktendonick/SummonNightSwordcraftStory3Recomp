# Portable Japanese and optional English — 2026-09-28

## Player-visible change

The main `release/Portable Beta/Swordcraft Story 3 Beta.exe` now supports the
original Japanese game without a translation. English is optional in Mods.
Home identifies the effective game language and the corresponding battery file.
The launcher interface itself remains English.

| Selection | Engine | Battery | Save-state prefix |
| --- | --- | --- | --- |
| Verified original, patch disabled | Swordcraft3Japanese.exe | Saves/japanese.eep | Save States/japanese |
| Original + supported English BPS | Swordcraft3CustomRendererBeta.exe | Saves/battery.eep | Save States/beta |
| Verified already-translated ROM | Swordcraft3CustomRendererBeta.exe | Saves/battery.eep | Save States/beta |

An already-translated file cannot be unpatched by toggling Mods. Select the
original Japanese ROM to return to Japanese. No save conversion is attempted.
Existing English saves/states retain their paths. Both variants keep the current
themed launcher, keyboard/gamepad controls and runtime menu.

## Implementation and contract

- `CMakeLists.txt`: optional `SWORDCRAFT3_PORTABLE_JAPANESE` target, fresh private
  Japanese generation using the current game symbol/config metadata and current
  recompiler. No English instructions are reused for Japanese. All 32 shards and
  dispatch table were compiled; generated debug information is omitted to reduce
  build size. Generated files are not manually edited or committed.
- `src/portable_language.h`, `src/main.cpp`: independently hash the effective ROM
  before selecting its matching engine. Correct the expected SHA/CRC to that
  verified revision, keep explicit diagnostic save overrides, propagate exit
  status, and retain clean-process resets. Ordinary non-portable single-engine
  paths keep their existing behavior.
- `src/beta_launcher_win.cpp`: verify both engines are present; no longer force
  every session into the English battery path.
- `gbarecomp/src/runtime/{runtime.h,launcher_seam.h}`: opt-in dual-engine launcher
  metadata, language labels and separate source/translated battery destinations.
- `recomp-ui/src/common/launcher_model.c`: optional unpatched acceptance does not
  relax source or patched-output identity checks. Changing ROM, toggling/clearing
  patches and restoring defaults update the visible save bank. Already-prepared
  targets are never patched twice. Commit the displayed save path to the host.
- `recomp-ui/src/common/backends/imgui/launcher_imgui.cpp`: language row and more
  specific blocked-Play guidance. Windows patch-cache path-limit errors now
  explain that the complete game folder should be moved to a shorter path.
- Packaging explicitly includes both engines, not ROM/BIOS/patch/player data.

Rendering algorithms, game scripts, camera rules and guest simulation were not
changed by this task. Existing authenticated rendering/fallback policies remain
in force. No new whole-game widescreen claim is made.

Build with `tools/build_custom_renderer.ps1 -JapaneseRom <private-original.gba>`.
Subsequent builds retain the configured private ROM path. The Japanese identity
is `3f5253fcf57e07ce52472bd29a61d16b98a12376`; the supported English output is
`bb2eebf98deb59bb6218442c2308bb5033ae2915`.

## Verification

All checks used game/model/control-flow state, not screenshots or pixel assertions.

- 29 selected CTest checks passed, including launcher, language routing, controls,
  runtime menu, rendering policy, codegen, save configuration and audio units.
  An initial test invocation lacked the compiler DLL directory; rerunning with
  the correct test environment passed all 29. That was not a packaged-DLL failure.
- Five archive-safety Python tests passed (payload changes, private inputs,
  settings and missing empty directories rejected).
- Actual original ROM + BPS model integration passed: mandatory-patch behavior
  for legacy single-engine hosts; optional original; exact English target;
  already-translated target; defaults/toggle/clear save-bank changes; incompatible
  IPS rejection; no changes to either language's battery fixture.
- Actual packaged launch tests passed: Japanese original, optional English BPS,
  switch back to Japanese, direct translated ROM. Each advanced at least 500
  guest frames, reported its correct ROM-specific runtime identity, and exited
  through the menu. These cold-boot runs reported fully static execution with
  zero dispatch misses. This is startup coverage, not a complete playthrough.
- An incompatible IPS was rejected before starting the game. The diagnostic
  harness's empty-output handling was corrected; the runtime never booted it.
- Japanese cold boot and an existing English combat state passed Pause/Resume,
  automatic pause on/off, manual pause latch, destructive-action cancellation,
  confirmed Close, clean-process Reset and preference persistence, with both
  present-in-place modes. The English state was never used for Japanese.
- Fresh ZIP: empty-input launch and normal close; no developer ROM, BIOS or save
  discovery; read-only preflight; packaged payload hashes unchanged.
- Actual control rebinding survived normal close and folder relocation with a
  system-only PATH and unrelated working directory.

Primary private evidence:

- `validation/pkg-40569019/Swordcraft Story 3 Portable Beta/Logs/language-tests-*`
- `validation/pkg-40569019/Swordcraft Story 3 Portable Beta/Logs/menu-tests-japanese`
- `validation/dual-final/Swordcraft Story 3 Portable Beta-moved/Logs/menu-tests-state`
- `build-native/Testing/Temporary/LastTest.log`
- `validation/pt/` (isolated model/import/patch fixtures)

The initial deeply nested test extraction exceeded the Windows patcher's C file
API path limit. Relocating the test package fixed that condition. The packager now
uses shorter test-extraction paths, and the launcher gives a specific error if a
player's patch-cache path would exceed the limit. Long-path/Unicode support across
all remaining file APIs is not claimed.

## Local package and installed update

Clean candidate:

`release/Portable Release Candidates/20260928T192607Z-40569019/Swordcraft-Story-3-Portable-Beta-Windows-x64-CANDIDATE.zip`

- 75,927,388 bytes; 53 allowlisted payload files.
- SHA-256: `2ed551fee19c1fc276d928220fe9b9e3bcd7fadbc8e5d06e70abef7a94f2a3ee`.
- Empty BIOS, ROMs, Mods, Saves, Save States, Settings, Captures and Logs folders.
- DLL closure, debug-stripping loaded-section equality, manifest and checksums
  verified. No archive was made from a played installation.

Installed only the top-level starter, both runtime engines and the updated README
into the existing `release/Portable Beta`. Backed up replaced files under
`release/Language Support Rollback 20260928T193455044`; its `UPDATE-REPORT.json`
records binary hashes and the 15 protected files whose hashes stayed unchanged.
Read-only installed-launcher preflight passed. No running player process was
stopped, and older candidate folders were not modified.

Installed SHA-256:

- Starter: `da84b367a17d545856aaeebfae7408904b4b68f53ec1244f8fe349632a704dae`
- Japanese: `25e3d72869b4c91622bb27daed052b704a8936a0d4a8e1b3759a6ce235e654c0`
- English: `2ad01188c9f5107919a06c96b6ee1e0364e63f07f7ae7d9f0d22ea679b075588`

No public release/tag/upload, commit, push or repository-visibility change was
performed. Source changes remain in the game worktree and designated engine/UI
feature branches. Existing distribution/provenance review in
`packaging/portable-beta/RELEASE-REVIEW.md` remains open. A clean-machine test and
broader Japanese gameplay/audio/controller coverage are still needed before
claiming comprehensive release validation or 1.0 readiness.
