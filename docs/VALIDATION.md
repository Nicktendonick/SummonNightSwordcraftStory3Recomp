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
continues from the live scrolling layers. The battle arena uses a game-specific
reflected-edge continuation because its 256-pixel ring leaves adjacent columns
empty; its HUD stays centered. Unsupported layouts fail closed to clean margin
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

Revised policy: reviewed field scenes now use a reflected nearest-edge
continuation; the 512px blanket rule is removed (unreviewed layouts fail
closed to pillarboxing); and the game opts into a new reusable
`g_ws_obj_native_clip` renderer flag in `gbarecomp` that clips OBJ pixels to
the native viewport. `ppu_smoke_tests` passes with a new opt-in/inert
regression test for the clip. True field continuation remains future work via
a map-data sidecar (see `gbarecomp/src/debug/ws_sidecar.cpp` for the
reference pattern and the csm3 notes below). The widened-mode margin captures
above predate this revision and must be re-taken; margin checks should also
compare against ground truth derived from map data rather than asserting
non-black pixels.

Battle registers (`SWORDCRAFT3_WS_DEBUG=1` dump: `dispcnt=3740
bgcnt=0000/450B/0305/080B winin=553F winout=553B`) show the arena art rides
BG1 on a 512px-wide map that the battle engine draws in full, so the battle
family now continues BG1 wrapped (real authored columns, not a stale seam)
and mirrors BG2. WINOUT excludes BG2, which previously blanked reflected
margins whenever a guest window was active; the reusable renderer now gates a
provider-remapped margin sample by its source column's window control instead
of the margin's WINOUT fallback. Rows beside the native-width HUD panels
show the continued arena backdrop while the HUD itself remains centered.

Headless Linux verification (sandboxed rebuild of the stock corpus,
`FULLY_STATIC` over the 4,400-frame canonical route): trace checkpoints
1,200/1,800/3,000/4,400 are title/menu/cutscene layouts and now pillarbox
with a pixel-identical native center (0 mismatches at 4,400). The English
beta field-dialogue save state renders 284x160 with zero black margin pixels
(mirrored scenery), and the first-battle state renders real continued arena
margins as described above. Stock-corpus runs of the beta ROM bridge one
translation-specific PC through self-heal, consistent with the known beta
coverage gap.

The rebuilt Windows beta target was then checked from the same field-dialogue
and first-battle save slots at 240x160 and 284x160. Both widened captures kept
the centered native image pixel-identical (0/38,400 mismatches) and contained
0/7,040 black margin pixels. The field used reflected scenery; the battle used
continued BG1 arena scenery with its reflected BG2 foreground. The battle run
still reported the two known dynamic-IWRAM self-heal misses.

Sidecar research notes (csm3): per-BG stream state lives at `0x030042C0`
(stride 20, shadow ring pointer at +0x10); the VBlank DMA queue at
`0x03002DC0` (count `0x03003180`) is flushed by `DmaCopyMapAndPltt`
(`0x08006AC4`); the strip streamers in `asm/code_copy.s` (`sub_08009840`
family) assemble 32-byte tile strips from a decompressed pool indexed by
12-bit map entries — that map-index array is the true-world source a field
sidecar should read.

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

The next deterministic trace must make the game-specific partner selection and
then cover exploration, map transitions, the first battle, in-game saving, and
post-battle transitions. Rendering/audio comparison against a reference
emulator and longer save-state/rewind soak tests also remain open.
