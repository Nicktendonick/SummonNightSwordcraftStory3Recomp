# Manig rocky arena and airborne framing — 2026-09-19

**Superseded detection:** the [September 21 state-hook change](BATTLE_STATE_HOOKS.md)
replaces map hashes, HUD color recognition and native-center comparisons in the
custom path. The original findings below are retained as historical evidence.

Status: locally built and validated; ready for owner playtesting. Not pushed.

The primary ongoing validation target is 12:5 (384x160), per the owner's
September 20 request. Do not run further 2:1 or 16:9 tests unless requested.

## Scope and cause

The owner identifies this as the rocky battle backdrop near Manig Mine where
the first slime is fought after crafting the first weapon. "Manig rocky arena"
is a descriptive profile name, not a separately verified official arena name.

The September 14 forest pilot explicitly rejected this pair of source maps.
The initial rocky addition authenticated it but retained a second problem:
the R-slot fix limited BG0 authentication/consumption to source Y 48..159,
the grounded footprint. Holding Up through the TCP debugger reproduced native
fallback during airborne frames, with `unreviewed-near-row` in the log.
Captured BG0 VOFS=14 exposes source Y=33 at gameplay row 19, outside that range.
This was a camera-dependent source-coverage error, not a need to change the
camera or disable arena authentication. It affected forest support as well.

The full input test also caught eight fallback frames across pause/unpause.
`battle_hud_borders.cpp` previously recognized only separator ends 19 and 59.
Actual captured intermediate ends are 27, 35, 43 and 51. These are now checked
with the same full gutter/separator authentication. Synthetic tests verify
native/gameplay pixels remain unchanged and malformed separators still reject.

## Source-owned correction

`custom_battle_identity.h` now authenticates source tile rows 0..19 of both
32-column near-map blocks: VRAM `0x3800..0x3cff` and `0x4000..0x44ff`.
This covers the 512-by-160 scenery while still excluding changing Guard/ability
glyph storage starting at `0x4500`. Per-line stability and consumption checks
use the same coverage; wrapped rows outside 0..159 still reject.

Paired near/far SHA-1 identities:

| Profile | Near rows 0..19 | Far allocation 0x2800..0x37ff |
| --- | --- | --- |
| Forest | `7f0309c30b95187d7b409e0a5ba7f73b2431694b` | `c25d865a7ad8e6c20d9804f6c2aacca32ff09a70` |
| Manig rocky | `a8ce0c8aa57c09f164966847722e2d9c627ac29d` | `71979aeb549c54f05620f4fda638614f28684d60` |

Standing and airborne rocky captures have the same expanded identity. Mixed
near/far pairs and unknown identities reject. Geometry alone never enables it.
The rocky far plane has a 384-pixel authored strip before repeated padding;
it uses the existing far-strip repeat policy with its own captured scroll.
Near scenery remains finite, regular attacks retain one unwrapped canvas,
and the existing GBA affine/priority/window kernel handles captured effects.
This does not invent terrain or enlarge the traversable arena.

## Contract and rollback

Game-owned changes only; no engine or UI submodule edits. Native-center pixels
must remain exact. Gameplay RAM, resident graphics/palettes and frame-boundary
cycles are checked; additional offscreen sprite submissions may change draw
bookkeeping. The native guest stays 240x160, camera/collision/AI are untouched,
and extra native replay remains off.

`Launch Combat Renderer Test.bat` enables forest and rocky support at 284, 320,
or 384 pixels. `Launch Combat - Forest Only.bat` sets
`SWORDCRAFT3_CUSTOM_ROCKY=0` for that process, retaining field/forest support and
the airborne correction. The original field-only launcher is also unchanged.

## Draw-state evidence

The original over-strict whole-state test stopped on IWRAM/OAM differences.
The pinned local csm3 `src/copy.c`, `asm/code_copy.s` and `linker.ld` identify:

- `0x030037a0..37af`: draw priority-list heads.
- `0x030038b0..3caf`: software OAM buffer, subsequently copied to hardware OAM.
- `0x03003cb0`: submission count; the original 128-entry check remains.
- `0x03003cc0..40bf`: submission pointers/list links (first six bytes per entry).
- `0x03004540..4b3f`: 96 draw records; flags, positions and priority occupy the
  first ten bytes of each 16-byte record. The reviewed builder writes these at
  08009BC6/08009BFE/08009E60 and onward before enqueuing at 08009EA2.

The extended cutoffs permit additional records instead of replacing their
position with hidden (240,160). The validator compares IWRAM every frame and
rejects differences outside these source-confirmed draw fields, rather than
ignoring all IWRAM. EWRAM, VRAM, palette and cycle hashes remain strict.

## Validation and limits

`tools/validate_manig_battle.py` drives the existing TCP server with isolated
battery saves and immutable source states. It compares old/new at equal width,
the native center on every frame, full output rollback, continuous widening,
and the declared guest-state contract. Private evidence stays under validation.

### Completed results

Final executable: `build-native/Swordcraft3CustomRendererBeta.exe`.
SHA-256: `c376471351bc258ff31fe7f72bf300e288e063f5592724d63c58ecbdf87e4c53`.
Build log: `validation/build-manig-pause.log`.
All 30 CTests passed (`validation/manig-final-ctest.log`).

| 12:5 validation | Coverage | Evidence |
| --- | --- | --- |
| Combat matrix | 11 cases, 1,438 frames per mode: rocky idle, R slots, pause, walking, B-button sequence, victory and jumping; forest idle, critical effect, jumping and pause | `validation/manig-final-384/` and matching `.log` |
| Actual attack/hit | 121 frames per mode; A-button attack visually confirmed, slime HP reaches zero | `validation/manig-attack-a-384/` and matching `.log` |
| Village regression | 60 frames; guest-state hashes, native center and full host output unchanged, including NPC shadows | `validation/manig-final-village/` |

Across the 12 combat cases, all 1,559 new-build frames passed native-center,
declared draw-state and rollback comparisons. All 1,547 complete frames stayed
wide; only the first incomplete restored frame in each case used native framing.
Rocky rollback matches the prior executable. Forest jump/pause rollback retains
their fixes because disabling rocky support does not disable those corrections.
Normal forest and forest-critical full output matches the prior executable.

Reviewed screenshots include the rocky jump, movement and animated pause panel,
plus the A-button swing and defeated slime. These are headless TCP-driven tests,
not a live-window performance or audio measurement. Earlier 284/320 checks also
passed before the owner selected 12:5 as the testing priority; no additional runs
at those widths are required for this handoff.

Failed intermediate runs are retained as diagnostic evidence, not counted as
passes: the initial overly strict whole-state comparison, airborne source-row
rejection, and intermediate pause-divider rejection. Their final replacements
are the completed runs above.

For owner playtesting, run `Launch Combat Renderer Test.bat` and choose
**3: 12:5**. Check the rocky encounter, jumping, R-slot cycling and pause/unpause.
Use F10 to capture any new fallback or visual issue. No commit, push or release
was performed for this change.

No whole-game, all-weapon/spell, JP-edition, live audio/pacing or fresh-entry
certification is implied. The first restored frame may be incomplete and native.
Forest critical effects were checked; rocky critical hits need further coverage.
The far-strip seam and finite near-plane edges remain authored-art limitations;
looping is not a claim that the original artwork was drawn for infinite width.
