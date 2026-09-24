# Casting animation continuation — 12:5

Follow-up to [the scripted Aqua Ball extension](AQUA_BALL_EXTENSION_20260922.md).
The later scripted spell already crossed the original screen boundary, but its
initial casting animation used a separate window producer. This change extends
that producer without changing game simulation, camera, native output, or HUD.
The separately reported rewind crash is **not addressed by this change**.

## Source ownership

The latest private captures are in `validation/playtest-20260922-164002-290`:
frames 61488 and 61517. Both record effect kind 3; the latter is cleanup with
BG2 already disabled. Neither is an active type-2 script descriptor.

- `0803B4C4` installs the casting actor at battle-root +454 and latches its
  screen position at +450/+452.
- `0803D68C` dispatches the effect kind to its window producer.
- `0803B91C` uses kinds 3/4, reading the current actor position at +19A/+19C.
  It computes `[X-56, X+64) × [Y-80, Y+40)`, then passes the signed endpoints
  through `0800493C`'s hardware clamp. Kind 5 disables this window instead.
- `0803B9C8` places the regular BG2 casting canvas using the latched origin:
  `HOFS = 184-X0`, `VOFS = 344-Y0`, masked to the captured nine-bit scroll.
- `0803B5F0` retires BG2 before retiring casting ownership. Its metadata can
  already be cleared while the captured display still shows the previous row.

For the reported right-edge capture, current and latched position are (220,120).
The signed rectangle is `[164,284) × [40,160)`, reproducing WIN0H `a4ff` and
WIN0V `28a0`; BG2 scroll is (-36,224), captured as (476,224). This authorizes
44 additional source positions per affected gameplay row, not a repeated or
stretched copy of the original image.

## Implementation and guards

`src/custom_battle_spell_window.h` authenticates five additional complete code
ranges, verified against both the owned JP ROM and English beta:

| Start | Length | FNV64 |
| --- | --- | --- |
| 0803B4C4 | 12C | f3512b952a15e4a6 |
| 0803B5F0 | 32C | b0710eeac0f686cc |
| 0803B91C | AC | 6e6d0e66d58ff4d8 |
| 0803B9C8 | 1C0 | ca3e511272601884 |
| 0803D68C | 88 | 426de01f756945d5 |

The decoder requires battle ownership, kind 3/4, a bounded active actor slot,
Mode 0, WIN0 without additional windows, reviewed window/blend controls,
valid alpha coefficients, exact scroll provenance and clamp-to-register
agreement. Enabled BG2 must use the reviewed 256-wide `0305` canvas. It does
not read the inactive type-2 descriptor, infer a scene from graphics, or gate
eligibility on a changing battle-menu submode.

Captured display state, rather than already-retired metadata, owns BG2 enable.
The casting window can remain valid while BG2 is disabled, preserving window
controls without resurrecting the layer. Only scalar bounds are retained for
each captured scanline. HUD rows refuse the extension, and native columns use
the original window test. The original 240x160 output is copied unchanged into
the 384x160 presentation. Source, sprite, blend and priority handling otherwise
use the existing renderer.

At the kind-3 to kind-2 handoff, the new script must authenticate independently.
No old casting rectangle is carried forward. In the reported continuation,
completed frame 40 has kind 2/state 0 while the old hardware WIN0 remains for
one more frame. BG2 is off and no active semitransparent OBJ remain. WININ and
WINOUT differ only in BG2 eligibility and color-effect enable; BLDCNT `1344`
targets BG2. There is consequently no effect consumer requiring the retired
casting window on that handoff frame. The validation asserts those facts,
rather than adding an exception for an inactive effect.

## Verification and limitations

The new `tools/validate_casting_extension.py` compares the preserved executable
with the rebuilt executable at 12:5. It checks captured register schedules,
guest memory/resource hashes, cycles, input, battle ownership and permitted
margin source positions. It includes cleanup, a new input-driven cast, the
previous type-2 extension, Start input during casting, and repeat restorations.
No screenshots, framebuffer comparisons or extra native replay are used.
Counts of source eligibility are not counts of opaque or visible pixels.

The latest capture exercises kind 3 at the right edge. An older capture and
an input-triggered cast exercise kind 4 with a native-contained rectangle.
Left-edge casting coordinates additionally have synthetic state coverage;
this is not a claim that every spell or encounter has been playtested. Visual
motion and appearance still need the owner's normal playtest.

The casting decoder and tests are game-specific. This step adds no engine,
launcher, generated-code, save/rewind or CPU changes. Prior uncommitted work
is preserved. No commit or GitHub push is part of this task.

Rollback executable: `validation/aqua-ball-20260922/casting-before.exe`, SHA256
`bab41e91e9a7a62f2ac4d10707d60a0d8827f131b584efd3f3446ee0105c2b52`.
It uses the existing build-folder runtime dependencies.

Rebuilt executable: `build-native/Swordcraft3CustomRendererBeta.exe`, SHA256
`d39f1d1af4b7801998ac91dd6e7a3004167a2f167349d101c4ee26450e112fe0`.

## Final results

- Eight focused CTests pass: spell ownership/geometry/retirement, battle layout,
  battle state, field animation/action/source, lake control and reusable window
  policy. The game-specific tests cover both edges, kinds 3/4, invalid actors,
  stale descriptor independence, signed overflow, window/blend guards and
  hardware-disable versus ownership-retirement transitions.
- `validation/aqua-ball-20260922/casting-verified/results/report.json` passes
  **870 paired frames** across eight cases. In the reported sequence, all 19
  active casting-BG2 frames authorize 44 right-margin source candidates on each
  of 85 gameplay rows: **71,060** candidates and zero opposite-edge candidates.
  Window ownership continues through frame 39, including 20 BG2-disabled
  frames without any BG2 samples; it retires at the new script's frame-40
  handoff. The later type-2 effect and prior left-edge extension are unchanged.
  Three independent restorations give identical first-60-frame guest and
  policy traces. All battle-owned frames stay wide.
- The Start-during-casting case preserves the original game's deliberate
  refusal to pause: `080270AC` routes mode 7 around the normal Start handler
  at `08027224`, which also requires effect kind 1. Both builds receive the
  two Start pulses in mode 7, stay unpaused and match state. This is **not**
  pause/resume coverage; normal pause is tested in the separate battle suite.
- `validation/aqua-ball-20260922/casting-battle-regression/report.json` passes
  **1,386 paired frames** in eleven cases: R cycling, jumping, normal pause,
  attacks, critical hit, victory/results exit and field-negative cases.
  Every recorded memory/resource hash and cycle count matches; no draw-storage
  exception is needed. Together these suites cover **2,256 paired frames**,
  not a complete CPU-register or full-game equivalence proof.
- The owner-folder `Launch General Field 12x5 Test.bat --check` succeeds.
  No interactive game was opened; all test battery saves were isolated and
  source captures remained unchanged. No FPS/audio claim is made.

The first `casting-final` run retains a failed test expectation that incorrectly
required casting ownership on the next script's inactive handoff frame. The
initial Start test also incorrectly expected pause during casting. Those
expectations were corrected using control flow, captured ownership and OAM
attributes; the game implementation was not broadened to satisfy them. The
final validator can resume only complete identity-verified runs and refuses
to overwrite partial evidence.

Low disk space briefly paused the checks. Lossless NTFS compression was applied
to private diagnostic folders and four preserved spell-test executables; no
files, captures or saves were deleted or moved. The owner then freed sufficient
space for verification to finish.
