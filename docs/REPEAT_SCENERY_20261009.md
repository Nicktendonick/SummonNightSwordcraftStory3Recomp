# Repeat scenery battle framing — October 9, 2026

A fourth **Battle framing** choice, after Current, Bounded and Follow + edge
stops. Launcher Mods and Esc > Graphics > Battle View list it as **Repeat
scenery**; it is stored as `battle_camera_mode = 3` in
`Settings/battle-camera.ini`. Older engines reject that value and use Current.

## Contract

- Framing is Current's: the native 240-column view stays centered, full host
  width, no side frame, no zoom. OBJ submission and decoding are Current's.
- Only margin samples of the near scenery (BG0) and far backdrop (BG1) change.
  The native center, HUD rows, effects (BG2/BG3), sprites, guest camera,
  perspective row scroll, actors, collision and AI are untouched.
- Exact-ROM arenas 0, 2, 3 and 7 only (the same ROM allowlist as Follow).
  Other arenas, field maps and
  native width keep their existing behavior. Current, Bounded and Follow are
  unchanged, including their far-backdrop loop.

## How a continuation is chosen

Plans are built from tile-map ENTRIES (tile index, flips, palette bank) read
from VRAM. They never read tile pixels, colors, framebuffers or screenshots.

A join between two tiles is **authored** when that exact left/right pair, or its
flipped image, already occurs somewhere in the map, so the original art is
continuous across it. Each 8-pixel tile row, on each side, uses one rule:

- `wN`: wrap through the outer N authored columns (an N-tile period);
- `mN`: reflect at the edge, ping-ponging over the outer N columns.

`src/battle_scenery_repeat.h` scores every rule per row over a 10-tile reach and
runs a Viterbi pass over the rows:

| Join | Cost |
| --- | ---: |
| Authored | 0 |
| Reflection in a row whose art already contains mirrored tiles | 1 |
| Wrap seam whose 1–4 preceding columns repeat (near period) | 12–18 |
| Any other new join | 20 (21 for a reflection) |
| Vertical tile pair, between rows using different rules, never in the map | 5 |

Ties prefer short wraps, then the widest reflection. The plan is rebuilt only
when the map entries change (once per arena; about 5 ms per layer).

The perspective floors have no authored horizontal continuation in any reviewed
arena: they are drawn as unique tiles, mostly as two mirrored halves. Their
rows therefore continue as a reflection at the edge, matching the seam the art
already has at its center. No new tiles or colors are synthesized.

## Plans

Read as rows top to bottom (near: 20 rows, far: 20 or 32). `unauthored` counts
non-authored joins inside the 10-tile reach on that side.

| Arena | Layer | Right | Left |
| --- | --- | --- | --- |
| 0 forest | near | rows 0–2 `w1` (open), 3–15 `w28`, 16 `m48`, 17–19 `w48` (reflection; symmetric rows); 1 unauthored | rows 0–1 `w1`, 2–15 `w28`, 16 `m48`, 17–19 `w48`; 4 unauthored (rows 12–15 tree bases). Since Oct 10: rows 0–15 inset 4, 2 unauthored (see Overscan insets) |
| 0 forest | far | `w20` (160 px, as before), uniform rows `w1`; 0 | same; 0 |
| 2 temple | near | all `w1`: open sky over its plain floor tile; 0 | same; 0 |
| 2 temple | far | rows 1–13 `w23` near period; 8 | same; 7 |
| 3 / 7 rocky | near | rows 0–2 `w1`, then `w48`: the whole arena repeats, floor rows meet as a reflection; 0 | rows 0–4 `w1`, then `w48`; 0 |
| 3 / 7 rocky | far | `w17` (136 px), rows 0–1 `w1`; 0 | rows 0–8 `w34`, 9–19 `w17`; 3 |

The forest rows were built by the engine itself from VRAM in the validation run
below (`[sc3:battle-repeat]` trace lines). Rows for arenas 2, 3 and 7 come from
the same planner run offline over the ROM's decompressed map entries; no save
fixture for those arenas was available, so they are not yet confirmed in-game.

Under Current/Bounded/Follow, the temple and rocky far backdrops wrap at their
whole 384-column strip (rocky: measured, no exact shorter period), and that
seam is non-authored in several rows. Repeat scenery uses the periods above.

## Verification

- `tests/battle_scenery_repeat_test.cpp` (synthetic entries only): invalid
  inputs, exact periods, open space over a floor tile, forest-like and
  rocky-like layouts, near-period seam choice, ping-pong mapping, hashing. An
  independent brute-force audit checks every horizontal and vertical join the
  plans create, and the per-pixel mapping is checked for continuity.
- Preference test now persists all four modes and rejects 4.
- All 30 game-repo CTest targets pass. The gbarecomp suite was not rebuilt; it
  is unchanged.
- `tools/validate_repeat_scenery.py` replays a copy of a supplied forest
  (arena 0) battle save state: 1,170 input frames walking to both guest camera
  limits (0 and 128), jumping and pausing. It ran in four cases: Current 384,
  Repeat 384, Repeat 284 and Repeat 240, on an English 1.1.0.f engine built
  from this source (that ROM identity is a separate change; the forest maps
  are the same):
  - every per-frame guest memory/cycle hash, camera and player position matches
    Current in all cases;
  - Repeat built exactly one near and one far plan, and was active on all 1,169
    owned frames with zero decorated columns;
  - continued near samples peak at both camera limits (4,646 per frame at 384)
    and fall to 0 at the centered camera; native width bypasses the compositor;
  - the state, ROM and BIOS were unchanged.

No screenshot, framebuffer or pixel assertion was used. Whether the seams look
right is the player's call.

## Overscan insets — October 10, 2026

A save state that starts in the battle introduction shows the forest with
taller HUD panels that frame exactly its tree-base rows, so the left seam
listed under Known limits became obvious. The cause, traced from the guest's
register writes (BG0HOFS/BG0VOFS per scanline) and the tile map:

- At camera C=0 the flat scenery band scrolls by Hflat=32, not 0. The native
  view starts at source column 32 (tile 4); the left margin shows source
  -40..31. The old plan joined the extension (tile 27) to tile 0 at source 0:
  that is the seam in the report, at host x=40 of 384.
- In rows 12–15, tiles 0–3 are unique: none has an authored left neighbour,
  so no continuation joined at tile 0 can be authored.
- Those columns are overscan. Authenticated `080352F4` scrolls every source
  row above the arena's perspective threshold d (ROM arena table byte 3:
  forest 128, arenas 2/3/7 136) by one flat offset,
  `Hflat = C + 8 + asr6((d-152)(C-64))`, for every vertical camera. For
  ordinary C=0..128 that is 32..112 (d=128) or 24..120 (d=136). Measured: the
  band ends exactly at source y=128 at rest (VOFS 32) and mid-jump (VOFS 14).
  So tiles [0, Hmin/8) and [(Hmax+240)/8, 48) of rows < d/8 are only ever
  seen in host margins.

`plan_scenery_repeat` now takes optional insets (rows, left, right). Each side
also tries starting its continuation 1..N columns inside the edge in the
inset rows, replacing those columns. Every candidate is scored over the same
absolute window (replaceable columns plus 10 tiles past the edge). An inset
is kept only when it scores strictly better, so periodic edges are unchanged.
The inset-free result is kept as `left_edge`/`right_edge`.

Each frame, `check_repeat_insets` reads the completed raster's BG0 scroll for
every scenery line on an inset row. If any line would place a replaced column
inside the native 240 columns, that side uses its inset-free plan for the
whole frame. The native view is never altered (margin callbacks only run
outside it).

Forest result (built in-game, identical to the offline planner):

| Side | Before | Now |
| --- | --- | --- |
| left | rows 2–15 `w28` at the edge; 4 unauthored (rows 12–15) | rows 0–15 inset 4, rows 2–15 `w28` from tile 4: the margin shows tiles 23–31 contiguous, then native tile 4; 2 unauthored (rows 14–15, at the native edge at C=0) |
| right | unchanged: 1 unauthored | no inset scores better; unchanged |

Rows 12–13 are now authored; 14–15 have no authored left neighbour for tile 4
either and keep one seam each. The vertical pair between row 15 (tiles 28–31)
and the floor row 16 (tiles 0–3, perspective, never inset) is new and counted.
The far layer has no insets. Arenas 2/3/7 get inset caps of 3 per side and use
them only if they score better; they still have no in-game fixture.

Planning is about 10 ms for the near layer (was 5 ms), once per arena.
Predecessors in the row search are visited cheapest first with an exact stop.

Verification:

- `tests/battle_scenery_repeat_test.cpp`: insets chosen on unique overscan
  edges, kept periodic edges, row limit, per-side fallback mapping, invalid
  insets, the native-view guard, and the brute-force audit now covers every
  join from 10 tiles left to 10 tiles right, including replaced columns.
- `tests/battle_follow_camera_test.cpp`: the flat band per source row against
  the scheduler transcription for d=128/136/140, every C and V.
- All 30 game-repo CTest targets pass.
- `tools/validate_repeat_scenery.py` now waits through the introduction and
  compares each Repeat case with Current at the SAME width. This save shows
  host-width-dependent guest state (Current 384 vs 284/240 differ from the
  left-edge walk on), which the old cross-width comparison wrongly attributed.
  Six cases, 1,440 frames each, both camera limits; Repeat matched Current per
  width on every guest hash; near plan left inset 4 in-game; the captured
  flat-band range was exactly 32..112; the fallback was never needed; insets
  served on 1,113 frames at 384.
- Investigation probes captured scroll writes (`GBARECOMP_MMIO_CAP=1` and the
  TCP `mmio_cap` command) and map entries. A diagnostic render of the maps was
  used only to understand the art; nothing asserts on pixels.

## Known limits

- Floors continue as reflections; a mirrored seam is continuous but visible as
  symmetry. On the forest's left, rows 14–15 keep one non-authored seam each,
  at source x=32 (the native edge at C=0); its right edge keeps one.
- The temple backdrop has no authored repeat; its near period leaves small
  variant seams in rows 5–12.
- Insets assume ordinary cameras. A special camera that scrolls the flat band
  past Hmin/Hmax switches that side to the inset-free plan for those frames,
  which changes the replaced columns while it lasts.
- Characters still stop at the original walls; the continued scenery is
  decoration beyond them.
