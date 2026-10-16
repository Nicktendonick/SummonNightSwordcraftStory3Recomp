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
- Exact-ROM arenas only (the same ROM allowlist as Follow): 0, 2, 3 and 7 at
  first, all of 0–16 since October 15 (see "All arenas"). Field maps and
  native width keep their existing behavior. Current, Bounded and Follow are
  unchanged, including their far-backdrop loop.

## How a continuation is chosen

Plans are built from tile-map ENTRIES (tile index, flips, palette bank) read
from VRAM. (Since October 15, joins the artist never placed are also
scored on tile edges; see "Tile-edge scoring" below.) They never read tile pixels, colors, framebuffers or screenshots.

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

## Arenas 2, 3, 7 and 8, tile-edge seams — October 15, 2026

Captures and save states from further into the game (mines arena 2,
rocky arena 3 during its introduction, village arena 8) showed the Oct 10 plans
were only right for the forest. All findings below come from game state: the
battle owner at IWRAM `03001A94`, per-line BG writes (`mmio_cap`), map entries
and the ROM descriptors.

### What was wrong

- **Gaps (arenas 2/3/7).** Their near art spans tile columns 3–44; columns 0–2
  and 45–47 hold one blank filler entry (`0x40`) in every flat row. Ordinary
  cameras never show them (Hflat 24..120 for d=136). Joins through filler
  count as authored, so the plans repeated blank (`w1`) or the whole 48
  columns (`w48`): a 24–48 px empty band where the backdrop showed through,
  ending in a straight cut at the art edge.
- **Re-planning (arena 2).** `08031420` loads a tile-animation asset for
  descriptor byte 2 (arenas 1, 2, 4, 13, 14, 15). Arena 2's near map cycles
  four entry sets, including cells two columns from each edge, so the plan
  key (an entry hash) changed every few frames: a ~10 ms re-plan each time
  (a mines capture ran at 54.7 fps) and rules that could switch.
- **Arena 8** is the village. It was never on the allowlist, so it fell back
  to native width.

### What changed

- **Padding.** Replaceable overscan columns that hold one entry in every flat
  row are padding: always replaced, never sourced, and joins with them are
  not authored. Replacing filler needs no native-view guard (the native view
  would show the game's own filler); the fallback plan replaces padding only.
- **One plan per battle.** Keyed by arena, rows and insets, invalidated when
  the battle owner ends. Animated cells still show live through fixed column
  rules.
- **Whole-column repeats, no mirroring.** The scenery band (rows above the
  ROM threshold d) shares one wrap period per side, chosen on the band alone.
  Every extension column is a copy of one authored column: nothing is
  mirrored, sheared or left as a stray fragment. Floor rows (perspective)
  still choose per row and may reflect, since floors are drawn as mirrored
  halves; the first floor row is not coupled to the band, because it scrolls
  in perspective against it. The far backdrop is one band. Ties prefer the
  longest wrap (the arena loops).
- **Tile-edge scoring.** Tile graphics are read for one narrow use only:
  choosing continuations. Never assertions, validation or scene detection.
  For joins the artist never placed, the planner reads the layer's 4bpp tiles
  (char base from BGCNT) and the BG palette from the same captured line, and
  scores how visible the seam is. Per pixel, the step across the join minus
  the natural step to the next pixel on either side (an opaque pixel against a
  transparent one counts 48). Foliage meeting foliage hides in its own
  texture; foliage cut against rock or sky shows. Authored joins stay free.
- **Arena 8 enabled** (`battle_uses_authored_384`): Ghidra shows it is the
  forest's descriptor family (threshold 128, far mode 1, no animation,
  effect or decoration assets) with 384x160 near and far maps.

Mirroring, floors forced onto the scenery period and an entries-only "loop"
preference were each tried and rejected in review. The flat-
layout previews were misleading, so review now uses composited in-game frames
at both camera limits (images for review only).

| Arena | Near left | Near right | Far |
| --- | --- | --- | --- |
| 0 forest | inset 4, band `w28`, floor `m48`/`w48` (unchanged) | band `w28`, floor `m48`/`w48` (unchanged) | `w40` (= the authored `w20`) |
| 2 mines | padding 3, band `w42`, floor `w48` | same | `w46` / `w47` |
| 3 rocky | padding 3, band `w41` | padding 3, band `w42` (loops into the other tree) | `w34` / `w17` |
| 7 rocky | as arena 3 | as arena 3 | as arena 3 |
| 8 village | band `w29`, floor `m48` | inset 2, band `w32`, floor `w40` | `w48` / `w16` |

In review the seams improved dramatically; a subtle seam remains on the
rocky arena's right edge, where two cut tree edges meet (no whole column in
the art matches better).

### Ghidra and arena inventory

Headless Ghidra 12.1.2 on the English 1.1.0f ROM: the
loader `08031420`, camera clamp `08031810`, scheduler `08031BC8`, scroll
builders `08031C88`/`080352F4` and readers `080261C4`, `08027464`,
`08031740`, `08034BD8`, `08034C24` are byte-identical to the Japanese ROM. The
camera clamp is [0, width-256] and the flat band uses only descriptor bytes
3–5, for every arena. Descriptor fields used: 0 palette asset, 1 near map,
2 tile animation (layer in byte 0x18), 3 threshold d, 4–5 width, 6–11
palette fades, 0x0C/0x0E archive-10 effects (params 0x0D/0x0F), 0x11 far
map, 0x12 far priority, 0x13 far scroll mode (byte 0x14/0x16 parameters; >9
fixed), 0x19–0x1B decorations.

Backdrop scroll modes (descriptor byte 0x13, decoded from `08031810`'s jump
table at `08031A48`): 0 and 5–8 fixed at byte 0x14; 1 = byte 0x14 × camera /
128; 2 = 1.5 × camera; 3 auto-scrolls byte 0x14 / 16 px per frame; 4 follows
the perspective formula (`08034C24`); 9 = mode 1 without the −16 vertical
offset; >9 fixed.

## All arenas — October 15, 2026

Every descriptor ID 0–16 is now widescreen (17–20 are identical fallback
records the matrix never selected naturally).

- `battle_uses_authored_384` lists 1, 2, 4–16; 0 and 3 keep their audited-ROM
  path. Arena 14's backdrop is 512x256 (`battle_far_source_width`), arena 15
  has none (`battle_far_source_rows` 0; near plan only).
- The layout gate takes the arena's backdrop from its descriptor
  (`BattleBackdropLayout`): BG1 enabled only with a far asset (byte 0x11),
  BG1CNT priority from byte 0x12. Arena 10's backdrop is priority 1 (a
  foreground hedge in front of the scenery); arena 15 has BG1 off.
- **Backdrop overscan.** Columns a backdrop's scroll can never show (from the
  mode table above and the camera range [0, width−256]) are padding: always
  replaced, never sourced, joins with them not authored. Arena 16's backdrop
  never scrolls (columns 30–47 unseen); arenas 11/12 never show their last two
  columns, which the old plan stretched as a 2-column repeat. Auto-scrolling
  mode 3 (arena 14) shows its whole map and claims none.
- **Padding above trailing floor rows.** Arena 5's last flat row (16) is
  already floor art across the full width; padding is now found over the band
  above it (up to two trailing rows), which then plan as floor.
- Arenas 11, 12 and 15 stop the camera near 105–112: the player's walls end
  before the clamp's 128.
- **Follow + edge stops** engages only where its jump envelope is derived
  (thresholds 128 and 136). On 1, 11–16 it keeps Current framing for the whole
  battle; before this, 13 and 14 (threshold 126) engaged on a few aliased
  Hflat values (24 of 862 frames) and could switch framing mid-battle.

Evidence (state only): an arena matrix (each ID selected through script
variable `0x1A1` in a lifecycle-2 fixture walked into an encounter) — all 17
IDs reach combat with 992/992 frames composed, Repeat active, one plan per
layer and no inset fallback; `tools/validate_repeat_scenery.py` per arena and
on real saves; and a framing matrix (Current, Bounded, Follow and Repeat on
the same route; gameplay identical, Repeat guest hashes identical, framings
inside the authored range).

Review notes (frames at camera 0 and 128): continuous in 0, 1, 2, 4, 5, 6, 8,
9, 10, 11, 12, 15 and 16; subtle seam on the right of 3 and 7 (two cut trees);
visible seams in 13 (lava clouds and orb cluster have no matching column) and
on 14's right (orbs cut at the art edge).

## Special cases and repeating floors — October 15, 2026 (evening)

Review of the all-arena build in play: the withered desert (7) is far less
seamless than the desert (3) despite sharing its art; the ruined village (6)
is not seamless at all; the lava crystals (13) have bad seams; the storm
crystals (14) have one on the right. The request: special cases that
leave the approved arenas untouched, and for floors that read as one
constantly repeating pattern, checked in every arena.

### Repeating floors (all arenas)

`plan_scenery_repeat(..., repeat_floor=true)`: the floor rows (from the ROM
threshold down) share one wrap period per side, chosen on the floor rows
alone, with no reflection. Per-row periods broke the perspective lattice
between rows, and reflections read as mirrored floors. The period is at least
four tiles (32 px): on arena 16 a two-tile period repeated an edge strip that
holds none of the floor's ice cubes, so the cube field stopped at the native
edge and the strip showed as stripes; eight tiles was tried and rejected
because it cut arena 8's pond off (its water repeats cleanly at five).

| Arena | Floor before (left / right) | Floor now |
| --- | --- | --- |
| 0 forest | row 16 `m48`, rows 17–19 `w48` | `w48` / `w48` |
| 8 village | `m48` / `w40` | `w5` (pond) / `w40` |
| 14 storm crystals | `m48` / `m48` | `w17` / `w10` |
| 15 water temple | rows 12–13 `m48`, then `w48` | `w48` / `w48` |
| 16 ice wind | `m48` / `m48` | `w19` / `w44` |
| all others | already one wrap per side | unchanged |

Scenery bands, insets and padding are identical with and without the option
in every arena (offline audit over all 17 VRAM dumps and the in-game plan
traces), and a test pins that property.

### Reviewed special cases (`src/battle_scenery_special.h`)

Two generic mechanisms in `battle_scenery_repeat.h`, used only by this table:

- **Sequences** (`scenery_repeat_apply_sequence`): a side's band rows follow a
  reviewed list of cells (column, optionally mirrored) in reading order,
  repeated outward from the edge left after its inset. One that replaces no
  more than the padding needs no native-view guard and is the fallback too;
  a deeper one keeps the planner's fallback for frames that would show a
  replaced column.
- **Quilted reflection** (`scenery_repeat_apply_quilt`): texture quilting's
  minimum-error cut, per pixel line. A narrow reflection continues the edge
  seamlessly; an unreflected run of the art takes over where the two agree,
  along an irregular boundary (steps of at most one pixel per line) instead
  of a straight seam. Error is the squared palette-index distance within one
  bank: arena 13's lava bank is a brightness ramp that cycles every few
  frames (tile graphics and map entries never change), so the cut does not
  depend on the palette phase at plan time. Each reflected pixel costs a
  little, keeping the reflection narrow. The margin callback now passes the
  source pixel line to `scenery_repeat_x`.

| Arena | Side | Continuation | Why |
| --- | --- | --- | --- |
| 6 ruined village | left | inset 3: `3–21`, then the same mirrored (edge cell `3f`) | the tower (3–7) is cut straight where the native edge hid it; mirrored at its outer edge (column 3 is a dark recess) it becomes one ruin with a doorway in the middle |
| 6 | right | inset 4: `43f…22f`, `22…43` | the cobblestone wall is cut the same way; mirrored at 43 (its overscan column 44 is a flat side that would show twice) |
| 7 withered desert | both | arena 3's band: left `3–43` (w41), right `3–44` (w42) | identical map entries; the withered foliage's own edge scores chose `w27`, cutting the right tree in half |
| 13 lava crystals (backdrop) | right | inset 4: authored columns 35–43, then `17–43` | 35–43 are beyond the backdrop's scroll range but continue the orb cluster; overscan padding had replaced them with lava |
| 13 (backdrop) | left | quilt: run from columns 0–17, cut within 40 px (engine picks run pixel 118) | column 0's lava edge matches no column or pixel run anywhere (best whole-column join 287, pixel-offset 255); a wide reflection reads as a Rorschach blot |
| 14 storm crystals | right | inset 4: `43f…36f`, `36…43` | the orb cluster runs off the map; orbs reflected through their middle stay round |

### Plan-time palette (bug fix)

Plans are built once, on the battle's first owned frame: intro frame 8 in a
natural battle start. Every battle fades in from black over frames 6–35 (BG
palette RAM mean brightness 0 → 54.8 of 93 in arena 8), so the tile-edge
scores were computed on colours at about 7% brightness. The arena matrix
(natural starts) and the review frames (combat-ready saves) therefore planned
some arenas differently (arena 8's band `w40` vs `w29`; arena 7 `w39` vs
`w27`), and real battles used the dark-palette plans.

The game's palette module (base `03005960` in IWRAM) keeps the tinted target
palette: `0800F2CC` writes each loaded bank to `base + bank*0x20`, and every
frame `08010CD4` copies the 0x400 bytes to `base+0x400`, applies the fade and
effects there (`080119D0`) and copies only that to palette RAM. During the
fade the target buffer already holds the full-brightness palette (all 512 BG
bytes equal the post-fade palette RAM). Scoring now reads that buffer's BG
half, captured with the battle state each frame; the routine's literal at ROM
`0x10D08` must equal `03005960` (it does in the Japanese and English 1.1.0f
ROMs), otherwise live palette RAM is used as before. Plan traces print
`palette=target` or `palette=live`.

Rejected in review: arena 6 looped with its authored gap column 21 between
the two cut faces (objects whole, but the straight cuts read as seams);
arena 13 lead-ins `10–18` (shows the small orb's edge) and `8–16` (straight
seam); a full reflection for arena 13 (mirror image).

Verification (state only; review frames were for review only):

- `tests/battle_scenery_repeat_test.cpp`, 23 cases: repeating floor (one
  wrap, no reflection), band independence from the floor option, sequences
  (reading order, mirrored cells pixel-exact, guard and fallback, invalid
  input), the special table (only 6, 7, 13 backdrop and 14; arena 7 equals
  arena 3's band), and quilts on synthetic tiles with known answers (direct
  join, deep cut, right side, invalid requests). All 32 game-repo CTest
  targets pass (the 28 gbarecomp entries are not built in this tree).
- Arena matrix on the final engine: all 17 IDs reach combat, 992/992 frames
  composed with Repeat active, one plan per layer, no inset fallback,
  `palette=target`; every plan equals the plan built from a combat-ready save
  of the same arena. Against the previously reviewed plans only the bands of
  6, 7, 13 and 14 and the floors of 0, 8, 14, 15 and 16 differ.
- `tools/validate_repeat_scenery.py` (Current vs Repeat at 384/284/240, every
  guest hash equal) passes on a forest save and on
  arenas 6, 7, 8, 13, 14 and 16. Its backdrop check is now inset ≤ padding
  (a reviewed case may keep authored overscan columns, never replace more).

## Known limits

- Floors repeat (no reflections since the evening of Oct 15); where a floor
  has no near-period its wrap keeps one seam per period. On the forest's
  left, rows 14–15 keep one non-authored seam each, at source x=32 (the native
  edge at C=0); its right edge keeps one.
- The rocky arenas' (3, 7) right edge keeps a subtle seam where two cut trees
  meet.
- Arenas 6 and 14 complete cut objects by reflection, so those objects are
  symmetric in the margins; arena 13's lava has a narrow reflected strip
  before its quilted cut.
- Special cases name columns of the exact-ROM maps; another map would get an
  odd-looking (never invalid) continuation.
- Ground details under repeated scenery (arena 8's pond and fences) need not
  line up with the scenery above them.
- Insets assume ordinary cameras. A special camera that scrolls the flat band
  past Hmin/Hmax switches that side to the inset-free plan for those frames,
  which changes the replaced columns while it lasts.
- Characters still stop at the original walls; the continued scenery is
  decoration beyond them.
