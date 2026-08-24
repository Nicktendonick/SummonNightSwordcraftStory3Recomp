# Validation status

Results recorded on 2026-08-09 using the `build-assist` RelWithDebInfo build,
the verified Japanese ROM, and the verified retail GBA BIOS.

## Assist Tools

| Requested mode | Presents | Guest frames | Guest frames/present | Guest FPS | Real-speed multiple |
|---|---:|---:|---:|---:|---:|
| 1x | 60 | 59 | 1.00 | 13.61 | 0.23x |
| 2x | 60 | 119 | 2.00 | 31.14 | 0.52x |
| 4x | 60 | 239 | 4.00 | 63.94 | 1.07x |
| 10x | 60 | 609 | 10.13 | 147.74 | 2.47x |

The selected multiplier controls guest frames per paced presentation correctly.
Actual emulation speed remains CPU/workload limited, so requesting 10x does not
guarantee 10x wall-clock speed on every scene or machine.

The deterministic Assist script saved slot 1, advanced, restored the snapshot,
built fresh rewind history, and rewound successfully. The snapshot also loaded
in two fresh strict-static processes; both 120-frame continuations ended at
guest frame 6,271 with zero dispatch misses and identical framebuffer hashes.

## Windowed call-depth regression

Windows Error Reporting identified the first-cutscene failure as an intentional
abort in `runtime_call_push_return`: present-in-place windowed execution had
grown the generated host call chain to its fixed 1,024-entry limit. The reusable
runtime now unwinds and redispatches at a safe VBlank when depth reaches 512.

A forced-threshold regression set the limit to 1 during a 120-presentation
windowed boot. It exercised eight safe unwinds and exited normally with zero
dispatch misses and zero interpreted instructions. The standard 4,400-frame
strict-static new-game route also remained `FULLY_STATIC` after the change.

## Native timing and audio

Results recorded on 2026-08-10 from the warm slot-1 checkpoint over 600
windowed presentations on a 165 Hz display:

| Presentation mode | Measured FPS | Present block p50 / p95 | Audio stretch at 8 s | Underruns / overflow drops |
|---|---:|---:|---:|---:|
| Renderer VSync (previous default) | 48.79 | 3,938 / 10,938 us | 1,565 ms | 0 / 0 |
| Native frame pacer (new default) | 59.76 | 250 / 1,745 us | 0 ms | 0 / 0 |

The reusable host now leaves renderer VSync off unless
`GBARECOMP_VSYNC=1` is explicitly set. `GBARECOMP_NO_VSYNC=1` remains a final
override for existing diagnostics. The frame pacer remains the sole normal
speed clock at the GBA's native 59.7275 Hz.

After this change, the forced call-depth regression passed and the canonical
4,400-frame new-game trace remained `FULLY_STATIC`. This checkpoint reproduces
the systemic frame/audio pressure but does not reach the first battle; that
scene remains a manual acceptance check until the deterministic trace is
extended through partner selection.

## Experimental adaptive widescreen

Results recorded on 2026-08-11 using the stock `build-assist` executable and
the canonical `new_game_select_male.trace` replay:

| Check | Result |
|---|---|
| Fixed wide surface | 284x160 (16:9), 22 added pixels per side |
| Adaptive sample surface | 262x160, 11 added pixels per side |
| Route checkpoints | Frames 1,200, 1,800, 3,000, and 4,400 rendered cleanly |
| Static execution | `FULLY_STATIC`; 0 dispatch misses and 0 interpreted instructions |
| Native-center preservation | 0 mismatches across the center 240x160 pixels at frame 4,400 |
| Shared renderer regressions | `ppu_smoke_tests` and `audio_drc_tests` passed |

An unrestricted WIP probe repeated/corrupted title and menu columns in the
added margins. The game policy now authorizes reviewed Mode 0 field/cutscene
and battle signatures in addition to visible 512-pixel tilemaps. Field scenery
continues from the game's complete source tilemaps retained behind the live
scrolling rings. The battle arena uses a game-specific reflected-edge
continuation because its 256-pixel ring leaves adjacent columns empty; its HUD
stays centered. Unsupported layouts fail closed to clean margin
pillarboxing. The launcher exposes Native, fixed 16:9, and window-driven
Adaptive modes, with Adaptive capped at 16:9.

English-beta save slots were also captured from the post-battle field dialogue
and the first active battle at 240x160, 262x160, and 284x160. Both widened modes
had zero black pixels in their added margins and zero pixel mismatches across
the centered native 240x160 image. These checks cover only those snapshots;
free exploration, map transitions, later battles, and effects that switch
display modes remain manual acceptance work before widescreen can be considered
production ready.

The English-beta executable also built successfully and rendered the
4,400-frame 284x160 checkpoint cleanly in its normal fallback-enabled mode.
Strict-static beta validation stops at translation-specific Thumb PC
`0x08012692`; a normal run bridged that one distinct PC (18 interpreted
instructions), healed it for the session, and completed the capture. That
coverage gap predates any production claim for beta widescreen and must be
reviewed in the beta static corpus separately from the renderer policy.

### Margin-policy revision (2026-08-11, later)

Free play on the English beta showed repeated/garbled tiles in the widened
margins. Root cause: the field policy kept wrapped 256px ring-buffer entries
(`kWsTilemapKeepWrapped`), but a single 256px screen block holds at most 16
valid columns beyond the 240px viewport, so a 262/284px view necessarily
rendered the opposite map seam plus stale streamer columns. The earlier
captures only asserted non-black margins and an unchanged center, neither of
which detects wrong margin content, and homogeneous checkpoint scenery hid the
wrap. Secondary contributors: the blanket acceptance of any visible 512px
Mode 0 layer exposed undrawn VRAM in menus/titles, and sprites parked just
off-screen (signed 9-bit OAM X) surfaced in the margins.

Historical revision: reviewed field scenes initially used a reflected nearest-edge
continuation; the 512px blanket rule is removed (unreviewed layouts fail
closed to pillarboxing); and the game opts into a new reusable
`g_ws_obj_native_clip` renderer flag in `gbarecomp` that clips OBJ pixels to
the native viewport. `ppu_smoke_tests` passes with a new opt-in/inert
regression test for the clip. True field continuation remains future work via
a map-data sidecar (see `gbarecomp/src/debug/ws_sidecar.cpp` for the
reference pattern and the csm3 notes below). This limitation was subsequently
removed for validated field descriptors by the true-map revision below. The
widened-mode margin captures
above predate this revision and must be re-taken; margin checks should also
compare against ground truth derived from map data rather than asserting
non-black pixels.

Battle registers (`SWORDCRAFT3_WS_DEBUG=1` dump: `dispcnt=3740
bgcnt=0000/450B/0305/080B winin=553F winout=553B`) initially suggested that
BG1's 512px map was fully authored. The later temporal audit disproved that
snapshot-based conclusion: after arena camera motion, BG1's left margin became
entirely black while the right remained populated. BG1 and BG2 now both use
safe nearest-edge reflection. WINOUT excludes BG2, so the reusable renderer
gates each provider-remapped margin sample by its source column's window
control. Rows beside the native-width HUD panels show reflected arena scenery
while the HUD itself remains centered.

Headless Linux verification (sandboxed rebuild of the stock corpus,
`FULLY_STATIC` over the 4,400-frame canonical route): trace checkpoints
1,200/1,800/3,000/4,400 are title/menu/cutscene layouts and now pillarbox
with a pixel-identical native center (0 mismatches at 4,400). The English
beta field-dialogue save state renders 284x160 with zero black margin pixels
(mirrored scenery), and the first-battle state appeared to render continued
arena margins. That isolated result was superseded by the longer route audit,
which found BG1's empty margin only after camera motion. Stock-corpus runs of
the beta ROM bridge one translation-specific PC through self-heal, consistent
with the known beta coverage gap.

The rebuilt Windows beta target was then checked from the same field-dialogue
and first-battle save slots at 240x160 and 284x160. Both widened captures kept
the centered native image pixel-identical (0/38,400 mismatches) and contained
0/7,040 black margin pixels. Those isolated snapshots predate the route-scale
BG1 correction and are not evidence that the whole arena map was authored. The
battle run still reported the two known dynamic-IWRAM self-heal misses.

Sidecar research notes (csm3): per-BG stream state lives at `0x030042C0`
(stride 20, shadow ring pointer at +0x10); the VBlank DMA queue at
`0x03002DC0` (count `0x03003180`) is flushed by `DmaCopyMapAndPltt`
(`0x08006AC4`); the strip streamers in `asm/code_copy.s` (`sub_08009840`
family) assemble 32-byte tile strips from a decompressed pool indexed by
12-bit map entries — that map-index array is the true-world source a field
sidecar should read.

### Deterministic route audit (2026-08-13)

The new route-scale auditor sampled frames 1,200 through 4,400 every 12 guest
frames in five aligned strict-static runs: Native composite, Wide composite,
Wide BG1, Wide BG2, and Wide OBJ. It retained 1,335 raw images. Every run was
`FULLY_STATIC`, with zero dispatch misses and zero interpreted instructions.
The derived report recorded zero capture-integrity errors, zero native-center
mismatches, and zero visual-detector findings.

All 267 Wide composite samples selected `pillarbox`. Five distinct scene
signatures were retained as safe fail-closed observations at frames 1,200,
1,272, 2,052, 2,592, and 3,912. This is useful evidence that unsupported
title/menu/cutscene layouts preserve the native image, but it is not authored
widescreen coverage. A new deterministic route through free overworld movement
and an active battle is the next required audit; the English-beta save-state
snapshots remain isolated evidence rather than a temporal route.

An earlier repeat was rejected because a connected Xbox controller triggered
rewind during only the Native run, leaving 38 missing samples. The reusable
runtime now supports exclusive input replay, and the audit also disables SDL's
controller backends. The accepted repeat opened no controller and contained no
rewind/save-state events. Raw evidence and the HTML report remain in the
ignored `validation/adaptive-widescreen/route-audit-new-game-03` directory.

See [WIDESCREEN_AUDIT.md](WIDESCREEN_AUDIT.md) for detector definitions,
limitations, the one-command wrapper, and the coarse-to-frame-by-frame review
workflow.

### English-beta field/battle route audit (2026-08-13, later)

The owner recorded a clean input route from English-beta Slot 2: 694 input
changes over guest frames 6,670..17,292, with no rewind or mid-route state
load. The route covers the complete first battle, Mode 0/Mode 1 combat-effect
transitions, the post-battle cutscene, and subsequent field/dialogue play.

The first coarse Native/Wide comparison exposed two audit-contract problems
and one real rendering defect:

- policy telemetry described the next rendered image but was attributed to the
  just-completed frame, producing one false transition leak; telemetry now
  records both the applicable PNG frame and its observation boundary;
- repeated per-frame seam alerts are now collapsed into persistent runs, and
  intentional black portrait stages are retained as safe observations;
- BG1's supposedly fully authored 512px battle map produced a completely black
  left margin after camera motion. Frame-by-frame BG1/BG2 isolation proved the
  source assumption wrong, so battle BG1 now reflects like BG2.

The exact 101-frame battle-entry repeat after the BG1 fix recorded zero blank
margins and zero center mismatches. A separate 91-frame battle-motion repeat
recorded zero blank margins, temporal freezes, fail-closed leaks, or center
mismatches across composite, BG1, and BG2 captures.

The final fixed coarse pass sampled 359 aligned Native/Wide frames over guest
frames 6,700..17,440. It recorded zero capture-integrity errors, zero center
mismatches, zero blank margins, zero temporal freezes, and zero pillarbox
leaks. Six persistent seam candidates remain; visual review identifies them as
the intended old-native-edge boundaries of centered battle HUD, dialogue, or
portrait art, and the report keeps them as review evidence rather than hiding
them. Seven safe observations cover fail-closed layouts and the intentional
black portrait stage.

This beta result is diagnostic-only. Native and Wide have the same execution
signature (`NOT_STATIC`, three distinct gaps, 36,810,454 interpreted
instructions), so the comparison is aligned, but it is not eligible for
release acceptance until the dynamic IWRAM/static coverage boundary closes.
The ignored authoritative report is
`validation/adaptive-widescreen/route-audit-beta-field-battle-fixed-coarse`.

### True-map field continuation and transition guard (2026-08-13)

The game keeps a complete source tilemap pointer, logical scroll coordinates,
dimensions, palette bank, and tile-base bias in four IWRAM BG descriptors at
guest address `0x03002A20` (stride `0x34`). The game-specific adapter now reads
those descriptors and resolves BG1..BG3 margin tiles from the complete map.
This bypasses the 32x32 hardware streaming ring, so widened field margins show
authentic adjacent map geometry instead of reflection or wrapped stale tiles.
The reusable runtime change is limited to read-only guest-memory exposure and
a game-owned final-frame presentation hook; all descriptor knowledge remains in
this repository.

An English-beta field replay over frames 10,060..11,060 selected
`field_true_map` for all 51 sampled frames. Native/Wide center comparison had no
mismatch or capture-integrity finding, and the post-guard Wide PNG hashes were
identical to the earlier good true-map run. The diagnostic run is still
`NOT_STATIC` because of the previously documented beta IWRAM coverage gap.

Owner testing also exposed a full-screen fire transition whose native center
was black while prior field scenery leaked through at the widened sides. The
transition is not present in the recorded route. Its screenshot measured 94.5%
near-black inside the authentic 240x160 viewport. Swordcraft's game-owned final
frame hook now extends black into the margins only when at least 90% of that
authentic center is near-black. This preserves normal field/dialogue frames and
requires a direct owner re-test of the fire effect for final visual acceptance.

## English beta BPS compatibility

The supplied beta patch was applied with the repository's reusable BPS engine.
All embedded BPS checks passed:

| Item | Result |
|---|---|
| Source size / target size | 33,554,432 / 33,554,432 bytes |
| Source CRC32 | `12AFAE5D` (verified Japanese ROM) |
| Target CRC32 | `A8F22FCA` |
| Target SHA1 | `bb2eebf98deb59bb6218442c2308bb5033ae2915` |
| Changed bytes | 364,766 in 17,522 contiguous runs |

This patch is not data-only. The first changed run starts at ROM offset
`0x1C50`, where it redirects a Thumb branch. Generating from the translated
target discovers 48,764 functions, compared with 48,708 for the stock ROM,
and produces a different dispatch table/static corpus. Running this beta with
the stock generated corpus would therefore execute stale instructions.

`SummonNightSwordcraftStory3RecompBeta` solves that boundary by privately
applying the user-supplied BPS during the build and generating a separate
matching corpus below the ignored build tree. Its launcher checksum-gates the
patched output to this exact beta release.

## Static coverage

- Canonical new-game route: 4,400 frames, `FULLY_STATIC`.
- Restored-state walk/interact continuation: 10,000 additional frames through
  guest frame 16,151, `FULLY_STATIC`.
- The continuation exposed two asynchronous return gaps. Reviewed resume ranges
  for `0x08003F9E..0x08004050` and `0x080060BC..0x08006106` fixed them.
- Final continuation image: `extended_walk_10000.png`; it is parked at the
  partner-selection menu.

## Remaining coverage boundary

The next strict-static stock trace must make the game-specific partner
selection and then cover exploration, map transitions, the first battle,
in-game saving, and post-battle transitions. The beta recording now covers the
visual first-battle/post-battle route, but its dynamic IWRAM bridge prevents it
from closing the static boundary. Rendering/audio comparison against a
reference emulator and longer save-state/rewind soak tests also remain open.

## Upstream runtime refresh (2026-08-20)

The game now targets `mstan/gbarecomp` main at `6571cb3`, with the reusable
deterministic capture controls and Swordcraft 3 map-provider hooks replayed on
top as `c3b1104` and `2cf1c0a`. Both the stock and English-beta executables
built successfully with one compiler worker.

The focused `swordcraft3_widescreen_route_audit_unit`,
`runtime_monolith_guard`, and `ppu_smoke_tests` tests passed. The deterministic
English-beta field/battle audit sampled frames 10060 through 11060 at step 20;
all 51 Wide composite PNGs were byte-identical to the accepted
`true-map-final-regression` capture. It retained the same seven known margin
findings and three dynamic IWRAM misses, with no native-center mismatch or
capture-integrity error.

`JRickey/gba-recomp` was evaluated as a separate Rust/C11 implementation, not
a source-compatible engine update. Its most relevant transferable performance
ideas are complete function-boundary coverage, profiling code copied to IWRAM,
bounded parallel translation, and differential verification. Closing this
beta's three dynamic IWRAM gaps is the most direct next experiment; its GPU
presentation architecture does not provide a drop-in widescreen optimization
for this C++ runtime.

## DKC audit transfer and hybrid map-edge acceptance (2026-08-24)

The English-beta route at frames 10060..11060 now records aligned Native/Wide
architectural hashes. All 51 samples matched for guest-visible CPU, memory,
video memory, I/O, audio, save, and clock state. The clean-history run retained
matching non-static coverage signatures in both modes (three misses and
36,810,454 interpreted instructions) and passed the route capability contract.

The true-map adapter previously returned transparency after reaching a complete
map's physical boundary. That caused five black-margin findings and a persistent
left native-boundary seam. It now uses authentic source-map tiles while they
exist and reflects the reviewed native edge only beyond the finite map. The new
capture has zero black-margin findings and no unreviewed map seam. One composite
edge remains allowlisted at frame 10060: it is the intentional end of native
dialogue chrome while the field continues behind it.

An evidence-only Tier-2 run enabled dynamic-RAM overlay healing. All 29 observed
targets healed, including `0x03003240` (621,421 native calls in the coverage
record); the warm Wide pass completed with zero interpreted instructions. This
is a promising performance result, not yet a release default: clean and warm
cache histories must remain separately reported.

Focused verification passed: the beta executable built against current official
`recomp-ui`, the widescreen-audit unit suite (10 tests), `ppu_smoke_tests`, and
`runtime_monolith_guard`. The reference-audio path now supports delivered S16
PCM dumps plus `gbarecomp/tools/compare_audio_pcm.py`; a trusted emulator PCM
capture is still required for a meaningful external audio comparison.
