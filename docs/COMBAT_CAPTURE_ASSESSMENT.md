# Capture assessment — 2026-09-14

Follow-up: the [R-slot fix](R_SLOT_FIX.md) addresses the confirmed identity bug.
The assessment below records the original reproduction before that correction.

Assessment only, requested by the owner. No renderer, executable, launcher or
save changes were made. Diagnostic output stays local under `validation/`.

## Captures inspected

| Session suffix / frame | Saved width | Finding |
| --- | ---: | --- |
| `215411-815` / 6754 | 320 | Recognized forest, tan HUD wings; finite foreground edge exposes distant scenery. |
| `215449-420` / 7788 | 284 | Recognized forest, expanded scenery and centered HUD. |
| `215538-152` / 5817 | 384 | Recognized forest with Guard selected; expanded margins. |
| `215538-152` / 5975 | 384 | Same forest, non-Guard selection; whole view falls back to native framing. |
| `215538-152` / 37049 | 384 | Rocky arena during VICTORY; unsupported by the new pilot. |

Full session prefix: `validation/playtest-20260913-`.
The newer `playtest-20260914-010216-718` contains an input trace but no F10
snapshot at the time of this assessment. No specific additional visual event
can be inferred from that trace alone.

## Confirmed bug: R selection changes arena identity

The existing TCP server reproduced the reported sequence from forest frame
5817 with combat widening enabled. After settling the state, press native R
(`KEYINPUT=0x02ff`) for 3 frames and release for 15 frames, eight times:

- Initial Guard selection: wide.
- Selections 1 through 5: native framing.
- Selection 6 (back to Guard): wide again.
- Selections 7 and 8: native framing again.

Logs report `battle=unknown-arena`, not `unrecognized-hud`, native replay
mismatch, or a cutscene flag. DISPCNT/BGCNT and the far-map identity stay the
same. The near identity at VRAM `0x3800..0x47ff` changes from
`9094eb22c6fcb297d698f2213cdf0a9983766e54` to
`4bf6267830ce6689d6ab8d4b472e5fd5c9e607c5`.

All 52 changed bytes in that 4096-byte identity range fall inside
`0x4500..0x45bf`, the six 4bpp tiles `0x228..0x22d`. These are referenced by the
native HUD map at entries `0x049c/0x049e/0x04a0` and
`0x04dc/0x04de/0x04e0`: tile columns 14..16, rows 18..19. They draw the lower
HUD's Guard label. Cycling to the empty selections clears that text to nibble
4. The actual changing data is font graphics, not a new arena.

Root cause: `CustomBattleScene::capture` authenticates an entire VRAM allocation
as if all of it were stable arena data. It accidentally includes shared/dynamic
HUD glyph storage. The earlier two accepted hashes only covered tested states.
Adding one more whole-allocation hash would fix this specific sample but remain
brittle against other labels, equipment, menus or translation text.

Recommended correction: separate stable authored arena identity from dynamic
HUD/graphics storage, retaining scene/layout checks and exact native-center
validation. Do not remove all guards or force the forest policy on every map.
Keep R cycling as a permanent regression test, including critical/pause states.

## Intentional limited coverage

The new custom-renderer route currently recognizes opening lake, village-chief
outdoors, adjoining village outdoors, and the forest combat pilot. These are
explicit game-owned profiles; window width is not universal content support.
Other overworld maps and battle arenas use native framing by design.

The rocky VICTORY capture has different near AND far identities, so its narrow
view is the intended unsupported-arena fallback. This capture alone does not
establish whether the victory overlay would need additional handling after
rocky-arena support is implemented. Older experimental renderer behavior does
not imply the new profile-based route already supports those same maps.

## Edge presentation and limits of evidence

The reviewed wide forest shots expose the finite near foreground ending while
the far backdrop continues. The current policy explicitly bounds the near map
and loops only the far plane, preserving separate scroll offsets. This explains
the visible distant-only wedges/strip near arena ends; it is not mirrored near
scenery. A camera constraint, intentional edge mask or authored extension would
be a separate presentation choice, not an automatic consequence of widening.

These static shots do not establish a new intermittent particle, sprite-culling
or parallax-timing bug. The R reproduction holds the scene wide/narrow according
to the changing glyph bytes, not scrolling. Do not claim all effects or all
parallax behavior are validated from these samples.

## Evidence / priorities

`tools/assess_combat_captures.py` loads all five original snapshots, captures
native and host images through TCP, reads live IO/VRAM and performs the R cycle.
`validation/combat-capture-assessment-20260913` contains the 384-wide run.
The snapshots' source hashes remain unchanged; every inspected host center
matches the corresponding native picture exactly. The initial report's frame
fields are null because screenshot replies omit frame IDs; the revised helper
uses the read-only `ppu_state` command to record them on future runs.
`validation/combat-capture-assessment-20260914-284` repeats the assessment at
16:9 with those frame IDs. It reproduces the identical Guard/non-Guard framing
sequence, with native-center equality and all source snapshots unchanged.

The running executable remains SHA-256
`7608a77b12c8c3c8bd9a213060b44c54b42df17da0bad64e0d4a89b23cbb2b52`.

Suggested order: correct R/arena identification, reduce the measured redundant
rendering cost (see `COMBAT_PERFORMANCE_BASELINE.md`), then expand arena profiles
and apply the same state/visual/interaction regression suite to each. Edge
presentation can be addressed separately without concealing identification bugs.
