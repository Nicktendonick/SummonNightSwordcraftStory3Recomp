# Aqua Ball: spell windows and battle cleanup at 12:5

Follow-up: [signed-window continuation](AQUA_BALL_EXTENSION_20260922.md) addresses
the owner's subsequent report that an active 512-wide Aqua Ball effect was
still clipped. The build and evidence below describe the earlier masking fix.

## Outcome and scope

Use the owner-folder **Launch General Field 12x5 Test.bat**. The rebuilt custom
renderer separates arena scenery's extended-view permission from spell-layer
window permission. It also keeps reviewed spell setup/cleanup states wide
instead of rejecting the entire battle view. Normal diagnostic logging is off.

The original 240x160 center is still copied unchanged into the 384x160 output.
No camera, collision, simulation, input, animation timing, guest hooks or
generated code were changed. The accepted generalized field animations and
field-tool/cutscene framing changes remain intact. No screenshots, pixels,
framebuffer comparisons or additional correctness replay were used as tests.

This fixes a source-permission defect reproduced from the owner's Aqua Ball
captures. It is ready for owner playtesting, not certification of every spell.

## Cause and implementation

In `playtest-20260922-145648-344`, frame 45213 describes a regular BG2 spell
canvas: BG2CNT `0305`, HOFS 71, VOFS 224, WIN0H `39b1`, WIN0V `28a0`, WININ
`3f37`, WINOUT `5513`, BLDCNT `1344`, BLDALPHA `1010`. WINOUT forbids BG2 and
color effects outside the rectangular window. The previous authored-background
Boolean bypassed the background-window gate for **all four regular layers**.
The effect's single-instance source interval could therefore reach negative X
despite its window, while the blend-enable gate still remained off there.

The reusable engine change adds `regular_bg_window_bypass_layers` to the replay
policy. Its default is zero; the existing authored-background Boolean retains
its legacy all-four-layer meaning, including the live runtime path. The game
selects only bits 0 and 1: arena scenery and HUD wings. BG2 uses each captured
row's normal window permission. Affine paths, blend equations, window selection,
DISPCNT enables and debug masks are unchanged.

A separate rejection happened when BG2CNT became `4305`, a 512-wide regular
canvas. Traces show this descriptor both disabled during cleanup and re-enabled
during a subsequent setup. Scenery ownership and its raster schedule continue.
Mode 0 now accepts this descriptor without treating it as a scene exit. That
BG2 canvas is kept **native-only**: its wider placement is not yet established,
so the existing 256-wide single-instance calculation is not applied to it.
Other layers remain wide. Mode 1 with this descriptor is still unsupported.

The enabled canvas eventually has a nonzero source extent; it is not claimed
to remain empty forever. This change deliberately does not implement a new
512-wide spell extension. Existing 0305/4385 paths and unreviewed-layout fallback
remain. The established source-span calculation still uses captured row 60;
arbitrary per-row spell-source changes are not independently covered.

## Final-build evidence

Private outputs are under `validation/aqua-ball-20260922`. Tests use isolated
loopback TCP processes and battery saves, read-only source states with hash
checks, and only width 384. There were no 16:9 or 2:1 runs.

- Seven focused CTests pass: battle window/layout, battle state, field
  animation/action/source, lake control and engine replay policy.
- The layout test exercises 655,420 register assertions, including all 16-bit
  DISPCNT values, legacy behavior and the scoped Mode-0 4305 exception.
- Engine tests count callback eligibility before tile decoding. They cover
  individual mask bits, WIN0/WIN1/OBJWIN selection, per-row WINOUT, layer/debug
  enables, modes 1/2/3, immutable captures and legacy live Boolean behavior.
- `paired-final/report.json`: five paired sequences, **534 frame samples**.
  IWRAM/EWRAM/VRAM/palette/OAM hashes, cycles, sampled battle/field metadata and
  traced raster registers agree between builds. This is not a full CPU-register
  or full-I/O equivalence claim.
- All 529 completed-frame state traces are contiguous; all battle-owned
  completed frames have 160 raster rows and stay wide. One restored mid-cast
  state has the same first unowned/narrow frame in both builds, before its
  ownership hook. It is not counted as a supported widened frame.
- In the primary 96-sample cast sequence, the prior build rejected **74**
  completed frames; the candidate rejects **zero**. The old policy allowed
  112,890 left-margin source positions despite the window; the candidate records
  zero accepted BG2 margin samples on all 2,560 window-forbidden rows. These are
  source-permission counts, not counts of visible or opaque pixels.
- A held-B continuation proves an input-driven effect transition: battle mode
  2 to 7, enabled 0305 canvas with extent 240 and the reviewed window/blend
  schedule, then disabled 4305 cleanup and return to mode 2. The test requires
  that ordered transition, not merely an enabled BG2. The spell's name is from
  the user's capture description, not an independently decoded spell-name ID.
- `general-battle-final/report.json`: **1,386 paired frame samples** across
  eleven cases: R cycling in two arenas, rocky/forest jumps, pause, attack,
  critical hit, victory, result exit and two field-negative cases. All recorded
  memory hashes/cycles agree; no special draw-storage differences were needed.

The original session input trace contains rewind discontinuities, so the five
sequences are bounded continuations from exact captures, not a claim to have
replayed the original recording. The new `validate_spell_windows.py` can rerun
them or recheck preserved results and executable/save hashes. Its optional
per-row metadata trace (`--window-audit`) is disabled during normal play.

## Identity, rollback and repository boundaries

Game branch: `experiment/custom-combat-renderer-20260913`, base commit
`b37e9d5f09c3252bf68e438859fb292e3cada271`, plus the uncommitted accepted field
animation work and this spell-policy/test/documentation delta.

Engine branch: `experiment/sc3-combat-renderer-20260913`, base commit
`5e8aa93cde0fdefbaa47303adfd98caf73f9618e`. Only `gba_ppu.h`, `gba_ppu.cpp` and
`replay_policy_state_test.cpp` changed in the engine. No UI changes.

SHA256 identities:

- Final `build-native/Swordcraft3CustomRendererBeta.exe`:
  `85608923a1b576b1dbe8a955330ef23a4f5c518cd13e9ec9b056024aa4886bf2`.
- Preserved accepted field-animation build, `before.exe`:
  `c63356f8d4a2f24ba3ebd3454a20f7d1d3286af7097ebf8448bc0c73be5a1eb8`.
- Trace-only baseline, `trace-before.exe`:
  `34d1da554dff3fd1fca63c8a21caa31dfbdcba2cf7e21081af66ff453bec21d5`.
- Owner's private beta ROM:
  `dbc6934925de2df75f94814f810c598519452b6aec75450b597e082adaaae735`.

Each probe's `identity.json` records its exact save hash, executable hash and
input sequence. Backups retain their existing runtime-DLL dependency; do not
replace an executable while it is running. The backup and all intermediate
evidence are preserved; nothing was deleted or pushed.

`paired/` and `general-battle-regression/` refer to the earlier prototype
`b41eca6f85bf52c79167171c372eb1e7ee30a1a248fe1c52ef3087711b920155`.
That prototype fixed the cast but still rejected the re-enabled 4305 canvas.
Its results are not final-build evidence; use the `*-final` folders above.

## Remaining checks

Owner playtest: cast Aqua Ball several times, watch the far-left margin and
cast/cleanup framing, then try other spells. This state-only validation proves
the permission correction and scoped non-regression, not every effect's
intended offscreen artwork. Native-only 512-wide spell canvases may still have
effects ending at the native boundary. No FPS, audio or real-window pacing gain
is claimed by this change.
