# Dark Hole affine battle layout — 12:5

## Cause and scope

Private capture `validation/playtest-20260922-171531-043`, frame 62549,
contains battle effect kind 9, stage 2. Battle ownership and the arena/HUD
schedule remain valid. The completed raster selects Mode 1, BG2CNT `0385`:
a non-wrapping 128x128 affine canvas. The old game-side layout allowlist
accepted the critical-hit 256x256 descriptor `4385`, but not `0385`.
`CustomBattleScene::draw` therefore rejected the whole frame with
`raster-layout-change`, leaving the original 240x160 presentation.

This is separate from both Aqua Ball's regular-background windows and the
translation's chest-message truncation. Neither the chest code nor the known
rewind crash is changed here.

## Source and lifecycle

- `0803BF6C` installs the actor/origin and effect kind, removes coordinate
  windows, and selects battle mode 7.
- `080391E8` dispatches kinds 9–12 to `0803BFFC`.
- `0803BFFC` selects Mode 1; `08004744(2, 0x80, 1, 0)` and
  `0800476C(2, 3, 1)` install descriptor `0385`. Kind 9 uses scale `0xB3`
  and an advancing rotation counter.
- `0803D68C` does not publish an Aqua Ball/casting rectangle for these kinds.
- `0803C2C6` retires resources; its temporary enable call precedes the
  caller's `0803D714` cleanup, which schedules BG2 off. Captured descriptor
  rows remain affine briefly after logical effect ownership retires.

These code ranges are shared by the supplied JP and English-beta ROMs.
The general resource metadata can say 256x256, but the captured hardware
descriptor owns the actual 128x128 affine sampling bounds.

## Implementation contract

`src/battle_layer_policy.h` now accepts exactly `0385` in Mode 1, without
coordinate/OBJ windows, subject to all existing scene ownership, arena,
BG0/BG1 enable/descriptor and HUD-schedule checks. It accepts both enabled
and disabled BG2 rows, so setup/cleanup do not depend on a transient menu
mode or a still-live spell-kind flag. Mode 0, wrapping `2385`, other sizes,
unreviewed windows, blanking and unrelated layouts remain rejected by this
new exception. Existing accepted layouts are unchanged.

The existing engine already handles this format. Its affine path extrapolates
captured signed coordinates into the margins, keeps each row's hidden affine
reference state, and rejects source coordinates outside [0,128) on either
axis. It does not apply the regular-background repeat/casting-window policy.
Window, alpha-blend and priority handling are unchanged. The native center is
still copied from original scanout; no extra native redraw is enabled.

This change is game-specific. It does not modify gbarecomp, recomp-ui, ROMs,
generated guest code, camera, collision, actor activation, saves or timing.
Earlier uncommitted changes in all repositories remain intact.

## Verification

`tests/custom_battle_window_test.cpp` checks active/retired affine rows and
negative layouts, exhaustively comparing old display combinations and exact
descriptor allowlists (917,578 state-only checks).

`tools/validate_dark_hole.py` uses isolated TCP-controlled runs at host width
384 / guest width 240, immutable source states and private battery saves.
It compares old/new memory/resource hashes, cycles, battle state, input,
all captured raster register traces, and retained IWRAM/OAM bytes. It asserts
continuous widescreen on battle-owned frames and permits only the intended
layout-decision delta. It never requests images or framebuffer comparisons.

The input-driven case starts at frame 62376, cycles R twice and casts with B.
It exercises kind 9 stages 1, 2 and 3, then restores ordinary battle state.
The old build falls back on completed frames 121–139: one setup row-frame,
16 enabled-canvas frames and two disabled-canvas cleanup frames. All 19
are wide in the new build. The reported late-effect snapshot additionally
tests seven previously rejected frames and three independent restorations.
Aqua Ball casting, cleanup and its left-edge script are separate cases.

Final results:

- Eleven focused state-only CTests pass, including battle register guards,
  spell windows, battle ownership, field animation/action/source and provenance,
  lake control, scenery identity, route auditing and engine window policy.
- `validation/dark-hole-20260923/paired/report.json`: 1,094 paired frames in
  seven cases pass, including the full input-driven cast, the reported tail,
  repeat restorations and Aqua Ball regressions. All checked guest state and
  captured registers match; only the reviewed layout decisions change.
- `validation/dark-hole-20260923/battle-regression/report.json`: 1,386 paired
  frames in eleven cases pass: R cycling, jumping, pause, attacks, critical
  hits, victory/exit and field-negative cases. No resource/draw-state exception
  is required. Combined coverage is 2,480 paired frames.

State checks are not an independent CPU oracle or a visual playtest.
The captured spell is kind 9; shared source handling for kinds 10–12 is not
a claim that those spells have all been playtested. This does not promise
support for every unknown spell layout or make an FPS/audio claim.

## Build and rollback

Use the unchanged owner-folder `Launch General Field 12x5 Test.bat`.
The private build is `build-native/Swordcraft3CustomRendererBeta.exe`:
SHA256 `c0afd54dfde67bc91724899c3b66da0413fd18128437f1e0473d06875de07be4`.

The previous executable is preserved at
`validation/dark-hole-20260923/before.exe`:
SHA256 `d39f1d1af4b7801998ac91dd6e7a3004167a2f167349d101c4ee26450e112fe0`.
It uses the existing build-folder runtime dependencies. To undo only this
source change, remove `small_affine_spell` from the layout predicate and
rebuild; preserve the earlier text-canvas and window fixes.

No commit or GitHub push is included in this task.
