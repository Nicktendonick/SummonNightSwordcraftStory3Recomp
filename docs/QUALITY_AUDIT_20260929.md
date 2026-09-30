# Targeted beta quality audit — 2026-09-29

**Subsequent implementation:** the main portable now contains the optimized
engines and a saved Guard option in Mods. See
[portable promotion](PORTABLE_PROMOTION_20260929.md) and
[Guard integration](GUARD_MOD_RELEASE_20260929.md). These close the corresponding
deployment/UI gaps below, not the remaining coverage or renderer limitations.

## Verdict and scope

Subjective engineering assessment: **7.5/10 for the tested private beta**,
**6/10 for 1.0 release readiness**. These are not percentages of the game tested,
security certification, or proof that undiscovered bugs do not exist.

This pass reviewed the current Guard/scaler changes, field/combat eligibility,
portable entry point, package recipe and existing integration evidence. It
reran selected tests; it did not run another gameplay session, audit every
function, inspect screenshots, judge audible quality, or check live GitHub.
No game code, installed engines, player settings, saves or release assets were
changed. This report is the only new durable source-tree file from this pass.

Source identity: game `96c0a3eaf52ec30372eaf7e9715deacc35f8f145`, runtime
`990abfcde6e7fa226393f0242a92644748f360c7`, UI
`3e2e059cac4fc6ecb5ccebdf25d697b5f640133a`. Game and runtime have the existing
uncommitted Guard/Smooth changes, including untracked implementation files.
The UI checkout was clean. HEAD alone does not describe this tested build.

## Findings, in priority order

### Release gate: main Portable Beta is not the optimized test build

Current file hashes were rechecked. The private Guard folder has the optimized
English engine `324057340aaa3667cb22118b65a63f720c1ba318d72c57a61417af0a12d1f4ee`
and Japanese engine `c72c0c7afd6c2e4a56760ae918a9f21ffce6df055c93a67362dc587ab27148bf`.
Main `release/Portable Beta` still has English
`c1570c220a5cda6d62e0cb1415cb06fa05ef5b6b5fd2b11369264e9f08733deb` and Japanese
`cf166934fc41aec1dce5f5d18b9b518cc865d5c3b23422fbe076bc36e48178bd`.

This separation was intentional, not an accidental regression. However, the
main install must not be described as already containing the Smooth optimization
or optional Guard. Never distribute the played Guard folder: it contains private
ROM/BIOS/save data. Promotion needs an explicitly prepared clean package.

### Medium: mine Select still causes unsupported-event width fallback

`src/custom_field_scene.h:220` permits free control, authenticated tool actions
and narrowly verified ambient events. Other scripts reject widening. The
slot-5 trace switches field flags from 1 to 4 after Select and falls back while
the requested host width remains 384. The later smoke test observed 30 wide
frames followed by 83 rejected frames. The original portable and Guard-off/on
comparison had matching guest hashes and ownership decisions.

The user's absent-partner explanation is consistent with Select starting an
event without a visible conversation, but is **not established by this audit**.
Read-only `references/csm3/asm/code_small_structures.s` at `080947B0` dispatches
the field's Select script ID at offset +6. `080A4564` changes the control flags
before starting the nonzero script. That proves the event entry, not the
contents of this particular script or its partner-presence test. Do not equate
the observed fallback with a verified missing-partner branch or a one-frame
glitch. Do not remove the script guard globally.

### Medium: widescreen is not universal across combat arenas

`src/custom_battle_state.h:43` accepts arenas 0, 2, 3 and 7 under the declared
state/register checks. User slot 3 is arena 8 and remains native fallback.
The latest playtest did exercise Guard/release there successfully. This is
missing renderer coverage, not proof that the battle itself is broken.

### Medium: Guard is still an experiment, not a finished Mods feature

`src/beta_launcher_win.cpp:78` enables the opt-in only for the separately built
Guard starter. `src/guard_experiment.h` implements the authenticated native
input/action substitutions; `src/custom_renderer.cpp` currently installs them.
There is no saved Guard preference, independent Guard binding or Mods checkbox.
Dropping a file into Mods will not activate it. The ordinary starter clears
inherited experiment flags.

Recommended eventual placement, not implemented: **Mods > Gameplay > Dedicated
Guard (Select, combat only)**, default Off, stored beneath the portable root's
Settings directory. State clearly that it replaces auto-battle activation in
normal manual combat, is hold/release rather than toggle, and leaves field
Select unchanged. Keep the behavior game-specific; the reusable RAM-write hook
must remain game-agnostic. Decouple feature configuration from renderer setup
before treating this as a general production mod.

### Medium: source provenance for a dirty release candidate is incomplete

`tools/package_portable_release.py:167` hashes `git diff HEAD` and records
`git status --short`. Git diff omits untracked file contents. With new files
such as `src/guard_experiment.h` and runtime `presentation_simd.h`, that pair
does not uniquely fingerprint the complete source used. The payload/binary
checksums still identify packaged bytes correctly; this is a reproducibility
gap, not evidence of archive corruption. The recipe also packages existing
build outputs rather than building them, so source freshness is a separate gate.

Before public release, use committed/pinned inputs and a verified fresh build,
or explicitly hash all relevant untracked source inputs as well. Do not claim
that a working-diff digest alone captures this experimental source state.

### Validation / distribution gates still open

- Japanese combat, a longer fresh-encounter playthrough, actual controller
  feel/focus transitions and perceptual audio checks remain incomplete.
- Earlier real-audio critical-effect testing recorded time-stretch events;
  zero bridge underruns is not a promise of crackle-free sound.
- The checked-in `packaging/portable-beta/RELEASE-REVIEW.md` remains OPEN for
  distribution provenance/notices/source availability and clean-machine tests.
  This audit does not settle those review items or offer a legal conclusion.

## Strengths and fresh checks

- Guard authenticates five surrounding ROM routines, exact instruction sites,
  actor, battle lifecycle and manual-control state. It uses native hold/release
  logic and retains no host-side Guard latch. Unknown layouts fall back.
- The generic write override is limited to bounded physical RAM, with width
  clipping; it does not replace MMIO, DMA, ROM or save writes.
- Smooth SIMD remains presentation-only, with scalar fallback and protected
  source-buffer tests. No gameplay clock or camera changes were added.
- The release recipe uses a fixed file allowlist, explicit empty player-data
  directories, payload checksums and dependency/PE checks.
- Build freshness check for 18 selected C++ test targets: Ninja reported no
  work to do. **20 selected CTests passed**, including Guard, SIMD/scalar,
  preferences, language, menu, portable input/launcher, scene gates and the two
  Python provenance/route entries. Existing pixel-based tests were not run.
- **5 ROM-free portable archive tests passed**, covering clean layout and
  rejection of changed payloads, private files, settings and missing directories.
- Game/runtime whitespace checks passed, apart from Git's informational
  line-ending conversion warnings. No fixes were made during the audit.

Fresh CTest details: `build-native/Testing/Temporary/LastTest.log` (this file is
overwritten by later CTest runs). The earlier, separately identified gameplay
evidence remains in [SHORT_PLAYTEST_20260929.md](SHORT_PLAYTEST_20260929.md):
3,364 inspected frames, five saved-display routes at 58.31–59.32 FPS, and
pause/resume/aspect/filter/state checks. Those are prior measurements, not new
FPS measurements made in this audit. See also
[GUARD_EXPERIMENT.md](GUARD_EXPERIMENT.md) and
[SMOOTH_PERFORMANCE_20260929.md](SMOOTH_PERFORMANCE_20260929.md).

## Suggested order, not authorization to implement

1. Authenticate and resolve the mine Select event's framing; separately reproduce
   the user's mixed-width appearance.
2. Finish the optional Guard UI/preference and document unsupported cases.
3. Expand arena 8 coverage, or explicitly disclose native fallback for the beta.
4. Test Japanese combat, real controller/audio behavior and a clean machine.
5. Complete distribution review, commit/pin approved inputs, build and validate
   a clean portable candidate before any public upload.
