# Custom combat renderer pilot — 2026-09-13

This is an opt-in forest-arena pilot, not support for every battle arena.
The accepted lake/village renderer and its normal launcher remain available.
The [September 14 R-slot fix](R_SLOT_FIX.md) separates scenery identity from
changing HUD glyphs. R selection no longer rejects the same forest arena.
The initial pilot's full-allocation hashes below are historical, superseded by
the source-row identity documented in that fix.

## Try it / turn it off

The extra native correctness redraw is now disabled by default for normal play.
Both combat launchers use that path. See
[the redraw comparison](NO_EXTRA_REPLAY_TEST.md). This does not fix the separate
R-selection issue or expand arena coverage.

Run `Launch Combat Renderer Test.bat` in `experiments/custom-renderer`.
Choose 16:9 first; 320- and 384-pixel host widths are also available. The launcher
uses the existing experimental playtest save and capture controls (F10).
It sets `SWORDCRAFT3_CUSTOM_BATTLES=1` for that launch only.

To return to field-only behavior, close the game and use the usual
`Launch Custom Renderer Lake Test.bat` in the main project folder. No permanent
configuration change, save conversion or camera-limit modification is required.
Other battle arenas deliberately retain native framing until verified.

Game branch: `experiment/custom-combat-renderer-20260913`, based on `5382dba`.
Engine branch: `experiment/sc3-combat-renderer-20260913`, based on `ab10a36`.
The original checkout's unrelated camera/tree experiments are untouched.

## Rendering contract

The reusable engine addition is an instance-local `GbaReplayViewPolicy` and
`GbaRasterCapture::draw_view`. A complete immutable raster capture is replayed
through the existing GBA wide pixel kernel. This preserves per-row registers,
palette, blending, priorities and affine behavior; it does not import a SNES
pixel renderer or advance the guest a second time. Replay does not install
global policies or invoke the live presentation/postprocess callbacks.

Game-specific recognition and composition live in `src/custom_battle_scene.h`:

- Authenticate both forest source maps, not merely their dimensions.
- Keep the finite near scenery within its source bounds, without mirroring.
- Loop the far backdrop using its measured source period and captured scroll.
- Keep separate layer scroll offsets for parallax.
- Render one unwrapped regular attack canvas, rather than repeating particles.
- Extend the existing battle OBJ draw cutoff into the reviewed host viewport.
- Keep original HUD pixels centered, with palette-derived tan wings and borders.
- Require every original 240-by-160 center pixel to match native replay before
  accepting an expanded frame. Unknown layouts, maps or HUDs decline safely.

The critical-hit teardown changes DISPCNT and BG2CNT non-atomically. Recognition
accepts the reviewed intermediate descriptors while retaining full map checks;
otherwise this produced a brief shrink despite unchanged scenery. Paused HUD
height is handled by the existing game-specific HUD-border helper.

Recognized SHA-1 identities:

- Near map, VRAM `0x3800`, 4096 bytes:
  `9094eb22c6fcb297d698f2213cdf0a9983766e54` or
  `e089b739f7985a4e626fb631fcfa978c4df7a22b`.
- Far map, VRAM `0x2800`, 4096 bytes:
  `c25d865a7ad8e6c20d9804f6c2aacca32ff09a70`.

This does not enlarge collision boundaries, change fighter activation, change
the battle camera or invent scenery/effect art. Authored effect/window limits
can still apply. Wider-than-384 host views are not enabled by this pilot.

## Validation

The included TCP server supplied input, stepped frames, native/host screenshots,
memory hashes and private save-state restoration. No user save was overwritten.
`tools/validate_field_objects_tcp.py --feature battles` compares the opt-in mode
disabled/enabled, with field objects enabled in both runs.

Project-local (ignored) evidence under `validation/`:

- `tcp-combat-critical-3`: 90 frames; no complete-frame fallback through impact
  teardown, unchanged native pixels and cycle counts.
- `tcp-combat-paused-2` and `tcp-combat-effect-3`: 90 frames each; expanded pause
  and attack views, unchanged native pixels and cycle counts.
- `tcp-combat-walk-169`: 180 input-driven frames at width 284; continuous wide
  complete frames and unchanged native pixels/cycle counts.
- `tcp-combat-unsupported-rocky`: intentionally no expanded margins.
- `tcp-combat-regression-village` and `tcp-combat-regression-chief`: accepted
  full-width visual reference hashes still match, including offscreen shadows.

The initial restored frame may lack a complete raster capture and stay native.
Additional guest draw data can differ; these checks do not claim full-state
identity. Seven focused CTests pass: lake animation/control, presentation/HUD,
runtime monolith guard, native raster capture, host presentation, and PPU smoke.
Capture tests cover all six native modes, exact replay-center parity, invalid
inputs and isolation from global presentation policy.

Test executable SHA-256:
`7608a77b12c8c3c8bd9a213060b44c54b42df17da0bad64e0d4a89b23cbb2b52`.

A 600-frame headless combat run also logged no complete-frame fallback or
native replay mismatch. Headless wall times are not a live 60-FPS/audio-pacing
test. User playtesting is still needed for movement, attacks, critical hits,
pause/resume, scene transitions and sound. No release or push was made for this
pilot yet; ROMs, BIOS, binaries, saves and capture imagery remain local.
