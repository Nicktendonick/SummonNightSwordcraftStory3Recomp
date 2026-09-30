# Guard mod integration — 2026-09-29

## Result

The user-approved Guard experiment is integrated into the normal local
`release/Portable Beta/Swordcraft Story 3 Beta.exe` workflow.
**Mods > Gameplay Mods > Hold Select to Guard** is a saved on/off checkbox.
The current owner's main installation is enabled; fresh packages default off.
No GitHub commit, release upload or public archive was made by this operation.

The native hold/release action, R-slot preservation, manual-combat/site/actor
gates and ROM routine authentication are unchanged from the tested experiment.
Off restores original Select/auto-battle behavior. On does not intercept field
Select or broaden renderer eligibility. Existing AI can still be cancelled.
No RAM polling/writing loop, synthetic timing, camera or damage change was added.

## Ownership and persistence

- Game: `src/guard_preferences.h`, `src/main.cpp`, and the pre-run setter in
  `src/custom_renderer.cpp`. Loads once before launcher initialization. The
  launcher callback saves atomically and only updates the displayed choice on
  success. `Settings/guard.ini` contains `[Launcher]` / `select_guard = 0|1`.
  It preserves other keys/sections/comments and supports BOM/Unicode paths.
  Missing defaults off; malformed or unreadable preferences fail closed.
- Runtime: optional pointer/count in RunOptions forwarded through the launcher
  seam. No game addresses, names or Guard implementation entered the runtime.
- UI: optional host-owned built-in mod descriptors, model validation and
  storybook-compatible checkbox panel. Independent of archive-mod compilation;
  zero-initialized hosts keep their existing layout. Translation patch remains
  a separate panel.
- Portable preference takes precedence over the historical environment flag,
  including explicit Off. The legacy flag remains useful for nonportable lab
  launches. Diagnostic tools seed both mechanisms when comparing old engines.
- Play reads the updated callback value; a language handoff or Reset Game reads
  the same portable file in its fresh process. No per-frame settings reads.

## Verification

- Rebuilt normal starter, English/Japanese engines and affected test binaries.
- 20 selected state-based CTest cases passed, including built-in Mods
  visibility, on/off persistence, Unicode paths, invalid data, read-only save
  failure and retained previous selection. Five archive-allowlist unit tests
  passed. This is not a claim that the whole upstream test suite was run.
- Actual copied normal starter on a Windows-only PATH: fresh Off -> On; reopen
  On -> Off; reopen stays Off; enable again. Empty game-input folders were not
  populated by that launcher-only test.
- Actual Mods -> Play with On and Off, in English and through the automatic
  Japanese-engine handoff. Each case executed a real confirmed Reset Game and
  confirmed Close Game in the child. Settings survived both; deliberate
  conflicting legacy environment values could not bypass the saved choice.
- Exact stripped staged engines: 20 input-driven regression cases / 2,017
  stepped frames passed. Guard hold/release/native actor state parity, six
  R slots, ordinary-input full-state equality, airborne/pause gates, AI
  cancellation, held-state restoration/release, field equality, and Japanese
  cold-boot/noncombat equality. Japanese combat still lacks a separate fixture.
- Real Esc menu pause/resume/run-behind, 240/284/384 host widths, Smooth off/on
  and private slot-9 save/restore passed. No presentation-filter fallback.
  Menu-paused runs are not performance benchmarks.
- Stripping preserved loaded PE sections, imports and entry point; packaged
  DLL dependency closure passed. No screenshots or rendered-pixel assertions.

## Installation and rollback

Five allowlisted files were installed: the normal starter, two engines, README,
and the new enabled preference. The obsolete
`Swordcraft Story 3 Guard Test.exe` was moved into the verified rollback folder.
All **131 other existing files** were byte-for-byte unchanged, including ROMs,
BIOS, saves, save states and prior settings. Private experiment folders were
not changed.

Owner-relative rollback:
`release/Portable Beta Guard Rollback 20260929-191544-e1cd`.
Its `UPDATE-REPORT.json` records all original/expected hashes. It contains the
replaced originals and retired starter; restore those and remove the newly
added `Settings/guard.ini` to return to the previous installation.

Worktree-relative evidence:

- `validation/guard-mod-release-20260929-191002-468d/PREPARED.json`
- `validation/guard-mod-release-20260929-191002-468d/SMOKE.json`
- `validation/guard-mod-release-20260929-191002-468d/INSTALLED.json`
- `validation/guard-1790709202305652300/report.json`

Earlier staging at `guard-mod-release-20260929-190249-ab1e` is superseded by the
Unicode-safe build above and was not installed. The first build attempt
identified a missing public UI-header include, fixed before passing builds.
Remaining mine-script width fallback, unsupported combat scenes, full-game
coverage, audible quality and distribution review are not claimed resolved.
