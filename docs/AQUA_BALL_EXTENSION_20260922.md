# Aqua Ball signed-window continuation — 12:5

The subsequent [casting-window extension](CASTING_WINDOW_EXTENSION_20260922.md)
adds the separate kind-3/4 casting stage; the results below describe the earlier
type-2 scripted effect change.

Follow-up to [Aqua Ball window masking](AQUA_BALL_WINDOWS_20260922.md).
The previous build deliberately kept unknown 512-wide spell canvases native-only.
The owner's new capture proves that the populated, active Aqua Ball canvas also
uses this layout. Its negative-X continuation needs an authenticated window,
not the scenery's broad window bypass.

## Source and capture evidence

Private capture:
`validation/playtest-20260922-160731-035/frame-0000015765-1790107822613`.
Recorded arena 0, phase 4, battle mode 2; host width 384, native width 240.

- BG2CNT `4305`, DISPCNT `3740`, HOFS 443, VOFS 216.
- WIN0H `0045`, WIN0V `28ff`, WININ `3f37`, WINOUT `5513`, BLDCNT `1344`.
- Effect kind/state at battle root +44E/+44F: 2/1, not the kind-3/4 actor window.
- Script owner `*(03000000+1E44)` is `02003200`; its inline BG2 descriptor is
  at +3C. Orientation 1, signed position (443,216), dimensions (512,256).
- The actor is `030008C0`, with its facing bit at +318 clear.

`0803CE14` dispatches regular BG2 scripts to `0803D5A4`; `080062C0` supplies
source-map dimensions. In the captured facing, the latter routine computes:

```
right  = map_width - signed_x           = 69
left   = right - descriptor_width - 1   = -444
top    = 256 - signed_y                 = 40
bottom = top + descriptor_height        = 296
```

`0800493C` then clamps signed16 coordinates into [0,255], producing exactly
the observed `0045 / 28ff`. The descriptor's logical window is therefore
**[-444,69) x [40,296)**, not [0,69). Ordered map entries in this snapshot differ
from the common `0040` entry in columns 52..63 and rows 0..11. Their candidate
screen interval is x[-27,69), y[40,136). This is source-entry evidence, not a
decoded-pixel/opacity claim. Extending a window does not stretch the spell art.

The source-aware helper follows all three reviewed descriptor orientations,
actor facing and the guest's signed calculations. It validates effect ownership,
pointer/actor bounds, source dimensions, captured scroll, window/blend controls,
and exact clamp-to-register agreement. It does not key eligibility to battle
mode 2 versus 7. Complete reviewed code-range fingerprints guard this ROM
interpretation; they match the owned JP and beta ROMs.

## Implementation contract

`custom_battle_spell_window.h` reads the small ownership/descriptor state at
each native raster observation. Only scalar signed bounds are retained alongside
that row's immutable register capture; no live guest pointer is used by replay.
Reset, load/rewind and battle retirement clear the sidecar.

The reusable `gbarecomp` replay policy now has an optional horizontal WIN0/1
margin-bounds callback. Native columns still use the original register tests.
Vertical bounds, WIN0/WIN1/OBJ-window priority and all six window-control bits
are retained, including blending enable. The callback runs at most once per
enabled window per row, not once per pixel. Its absent/invalid case preserves
existing behavior and it does not modify the live renderer's global policies.

The game supplies WIN0 continuation only in the authenticated battle gameplay
band. BG2 samples stay inside that one logical rectangle. Source coordinates
still use the captured scroll and texture-map layout. Unauthenticated 512-wide
effects remain native-only; the 256-wide nearest-copy heuristic is not applied
to them. Other spell kinds, affine effects, HUD wings, finite terrain and field
framing keep their existing policies.

The native 240x160 center is copied unchanged into the 384x160 presentation.
There are no guest writes, simulation changes, extra replay checks, generated
code modifications, screenshots or pixel-based assertions in this step.

## Timing and limits

The new trace exposed a retirement ordering issue: descriptor active flags can
clear while the old BG2/window is still displayed. The shadow display register
already requests disable, but captured live IO has not yet applied it.
`0805A51C` clears the descriptor flag, resets scales/counters and calls
`08006210 -> 08005B5C`, clearing 0x32 bytes of BG metadata while preserving
the script's signed coordinates, orientation and extents. Its two padding bytes
are not cleared. These three complete routines are also fingerprinted.

The helper accepts this final displayed frame only with that exact reset
signature, fully cleared BG metadata and a shadow display value equal to the
captured value with BG2 and WIN0 disabled. Dimensions then come from captured
BG2CNT. Signed descriptor scroll/window equations must still reproduce the
captured registers. Once captured BG2 disables, the extension stops. No
previous-frame rectangle or extrapolated history is needed. This is a
display-state ownership issue, not a new battle mode.

This is support for the traced type-2 script-window mechanism, not a universal
promise for all spells. Opposite-facing bounds have synthetic coverage; the
owner's new reported capture covers the left edge. User playtesting remains
necessary for appearance, motion, other spells and real-window pacing.

Rollback checkpoint: `validation/aqua-ball-20260922/window-clipped-before.exe`,
SHA256 `85608923a1b576b1dbe8a955330ef23a4f5c518cd13e9ec9b056024aa4886bf2`.
It uses the existing build-folder runtime DLLs. All earlier evidence and saves
are preserved; no commits or GitHub push are part of this change.

## Final-build verification

Built on game branch `experiment/custom-combat-renderer-20260913` and engine
branch `experiment/sc3-combat-renderer-20260913`, preserving the accepted
uncommitted field animation and preceding spell-mask work. Game-specific
reconstruction lives in the game repository; the optional replay primitive and
its tests live in `gbarecomp`. No launcher UI changes.

`build-native/Swordcraft3CustomRendererBeta.exe` SHA256:
`bab41e91e9a7a62f2ac4d10707d60a0d8827f131b584efd3f3446ee0105c2b52`.

Private evidence under `validation/aqua-ball-20260922`:

- `signed-edge-final-v2/report.json`: 120 paired frame samples from the new
  capture, 119 consecutive completed frames. All battle-owned frames remain
  wide. The signed window is present for **all 40** displayed effect frames,
  including pending retirement, then absent when the effect disables. Across
  3,400 gameplay rows it authorizes 244,800 left-margin source candidates and
  **zero opposite-edge candidates**. These are permission counts, not visible
  pixel counts. Sampled RAM/VRAM/palette/OAM hashes, cycles and captured raster
  registers are unchanged.
- `signed-cast-regression/report.json`: the five earlier cast/cleanup and
  held-B input sequences pass, 534 paired frame samples. Exact input-driven
  mode/effect/teardown ordering is required. The original erroneous opposite
  margin access remains forbidden; authenticated type-2 rectangles are now
  allowed instead of asserting every windowed effect must be native-only.
  One restored mid-cast state still has the same first unowned frame in both
  builds, before the ownership hook. This is not claimed as active-wide coverage.
- `signed-battle-regression/report.json`: eleven cases, 1,386 paired frame
  samples, all passing with identical recorded memory hashes/cycles. Covers
  R cycling in two arenas, rocky/forest jumps, pause, attacks, critical hit,
  victory/result exit and field-negative cases. Together the final-build suites
  cover **2,040 paired frame samples**. No draw-storage exception was needed.
- Eight focused CTests pass: signed spell ownership/retirement, battle layout,
  battle state, field animation/action/source, lake control and engine replay
  policy. The new game test includes both facing directions, signed bounds,
  invalid sources and exact pending versus committed cleanup. The engine test
  exhaustively preserves native horizontal membership for all 16-bit window
  registers, and checks priority and all six window-control bits without reading
  raster output.
- The normal owner-folder launcher passes `--check`; no interactive game was
  launched. Original saves remain unchanged; each TCP run used an isolated
  battery save. Only 12:5 was tested. These checks are not a full CPU-register
  equivalence test and do not establish an FPS/audio improvement.

`signed-edge-final/` records the earlier prototype
`0ae9b6e589436591fdda4c254baa41075f608d8d03b858b6de60bbd5a10b4224`,
which lost the last displayed frame because its metadata had retired. It is
preserved as failed intermediate evidence, not the final result. The additional
RAM trace in `edge-retirement-state/` explains the correction.
