# Battle backdrop looping and single-copy effects (2026-09-09)

The owner clarified the desired presentation with a looping mountain-backdrop
example: distant scenery should match and loop over the full battle width.
This supersedes the earlier experiment which clipped that scenery to the
near-arena silhouette. Foreground terrain is NOT looped or mirrored by this
change. Overworld black transition boundaries are untouched.

## Use / rollback

Restart `build-beta/SummonNightSwordcraftStory3RecompBeta.exe` for English beta,
or `build-beta/SummonNightSwordcraftStory3Recomp.exe` for Japanese.

`Launch Beta - Previous Battle Layers.bat` sets
`SWORDCRAFT3_LEGACY_BATTLE_LAYERS=1` for one session. This preserves the recent
tan HUD/seam corrections but restores the previous battle-layer behavior.
No configuration or save is overwritten by this switch. The pre-change beta
is also preserved under `validation/battle-layer-fix-20260909/previous-runtime`.
The original Alpha review archive is unchanged.

The new policies apply to the default natural battle mode. Existing explicit
`SWORDCRAFT3_BATTLE_MARGIN_MODE=loop` / `reflect` experiments remain legacy
alternatives and intentionally do not use the new contextual override.

## Confirmed causes and implementation

- BG2 carries part of the attack animation. In captured frame 7550 the native
  attack canvas was repeated 256 pixels away by hardware-coordinate wrapping.
  The game adapter now selects a single unwrapped effect interval, extending
  that copy only. Sprites remain a separate layer.
- BG1 is the distant scenery. Its 512-pixel allocation has a 384-pixel authored
  prefix, but the forest's genuine repeated pattern is **160 pixels**. Exact
  tile-column comparisons verify that period; hashes only shortlist candidates.
  The margin sampler wraps through that cycle, not through padding, the native
  viewport, or a reflected edge. Maps without a verified shorter cycle use
  their complete non-padding strip; seamlessness of every other arena is not
  certified by the forest test.
- Scroll values and tilemaps can change after the frame-start adapter snapshot.
  A new nullable runtime hook supplies the actual rendered scanline's IO and
  VRAM to margin sampling. BG1 keeps that scanline's scroll phase and its normal
  vertical scroll, palette, blending, and layer priority. No guest camera value
  is changed, and no native-center sample goes through this hook.
- The renderer's authored-margin bypass accidentally also bypassed debugger
  layer selection. That is corrected, so BG-only/OBJ-only captures really are
  isolated in the extra columns too. Earlier layer-isolation screenshots had
  contaminated margins; old on/off equality checks did not detect this.

## Repository boundaries

Game-specific rules live in `src/adaptive_widescreen.cpp` and
`src/battle_layer_policy.h`, with checks in `tests/test_battle_hud_borders.cpp`.
The reusable scanline-context hook, runtime reset, layer-mask correction and
PPU tests live only in `gbarecomp`, on branch
`fix/widescreen-scanline-margins-20260909`. `recomp-ui` is unchanged.
Changes are local/uncommitted; nothing has been pushed or released.

The shared `g_ws_bg_margin_provider` is opt-in. It sees only extra columns,
borrows read-only IO/VRAM for the callback, and returns suppress, delegate, or
an authoritative horizontal sample. Native rendering never invokes it, and
the default null hook preserves other games' existing rendering paths.

## Reproduction

`tools/validate_battle_layers.py --output-dir validation/NEW-DIRECTORY
--previous-executable PATH` clones a runtime into a fresh private directory,
replays the owner's attack/pause captures plus village/overworld fixtures, and
checks old/new/rollback at 240, 284 and 384 pixels. `--motion` replays the owner's
recorded attacks, movement and pause sequence from frames 7552 through 8152,
sampling every 40 frames. Input states are read-only and hash checked.

The forest BG1-only check compares every extra gameplay pixel against the
same row's native sample modulo 160, proving both matching edges and phase.
The BG2-only check asserts that the duplicate right-hand flash is absent.
All cases compare full native centers and guest-state traces; rollback must
match the previous executable's full composite image exactly. Protected
overworld images and normal/paused HUD bands must remain unchanged.

Local evidence: `validation/battle-layer-fix-20260909/` (never distribute).
The `quick1` trial fixed effect duplication but did not meet full backdrop
coverage; `quick2` uses the owner's clarified full-loop presentation.

## Remaining limitations

BG2's 9-bit scroll can represent multiple placements of the same canvas.
The current rule chooses the copy with the largest native-screen overlap;
ties do not extend. Completely off-native attacks and all spell families need
further tests or a proven game-owned effect anchor. This is not a fix for every
possible sprite pop: the separate signed-OAM-X lead remains unmodified.

The existing forest Native/Wide guest-state discrepancy is outside this change.
Same-width old/new parity is not a fully-static or whole-game certification.
The Japanese target is compiled and unit-tested, but replay evidence here uses
the English beta. Live resize, GPU/vsync performance, and other arenas still
need owner play-testing.

## Build identities and performance check

English-beta SHA-256:
`33D59B0EF7CF3FCE8FF7BD955C03B0A7CCEF9AD6AFD0FB17835716B1AECAC9BC`

Japanese SHA-256:
`E94AA2EEF500AD6FF872EA21D12AE8854FCD5340455B53F14FC2A3E75DCB6E2E`

Both editions rebuilt; four focused CTest checks passed. Map extents and the
repeat period are cached per frame, measured from the first gameplay sample
after VBlank uploads. Scroll registers and pixel composition remain live per
scanline. Arbitrary games that rewrite the tilemap extent mid-visible-frame
would need a different game policy; the reusable hook exposes their live data.

The sequential dummy-video/audio comparison in `performance-final/report.json`
sampled 583 frames of the same recorded route at 384 width. Mean measured work
was 17.74 ms before and 18.55 ms after; medians were 17.92/19.04 ms. This is a
host-dependent diagnostic (not a GPU/vsync or locked-60-FPS certification).
There is still extra rendering cost. The earlier `performance` experiment ran
alongside the motion regression and is not a controlled comparison.

Final optimized-build validation passed:

- `final/report.json`: attack, paused battle, village and overworld at
  240/284/384, three frames each. Native centers and guest-state traces match
  the pre-change executable; full composite rollback is exact. Every forest
  backdrop margin pixel passes the 160-pixel phase comparison, and the
  duplicate BG2 flash is absent. Overworld and HUD checks pass.
- `final-motion/report.json`: frames 7552..8152 sampled every 40 frames at
  240/284/384 using the owner's actual input recording. Native-center/guest
  parity and full-image rollback all pass through attacks, walking and pause.
- The validated runtime copy's hash equals the current `build-beta` executable.
  Raw frames, state traces and inputs are private and ignored by Git.
