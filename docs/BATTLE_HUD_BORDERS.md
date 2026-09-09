# Battle HUD borders

Game-owned presentation experiment, started 2026-09-08 from the owner's tan
border mock-up. No changes to gbarecomp or recomp-ui are required: this uses
their existing final-frame callback and game-owned runtime-menu item hooks.

## Use and undo

The latest local executables are under `build-beta`. Both the English-beta
and Japanese targets include the option. Press **Escape → Display → Battle
HUD borders** during play. This first implementation adds the row to the
in-game menu, not the pre-boot launcher. Native view is unaffected.

The initial preference is on. Toggling it saves just this game-owned preference
to `swordcraft3-display.ini` beside the executable, shared by the two variants.
Save-state loading and rewind do not replace the host preference. Startup only
reads the file; the menu writes it when changed. A write failure is logged and
the current session still gets the requested value.

`Launch Beta - Previous HUD Margins.bat` sets
`SWORDCRAFT3_BATTLE_HUD_BORDERS=0` for one process. Values `0` and `1` override
the stored initial preference and prevent persistence for that session; they
are also the deterministic audit controls. Off removes the tan HUD extension;
since September 9 it retains the one-row BG0 raster seam correction. It is no
longer a pixel-exact capstone rollback. No save format changed.

The Alpha unfinished-build v.01 review archive remains untouched and does not
contain this feature. Its pinned packager will intentionally refuse the new
working executable until a separately reviewed recipe/build identity is chosen.

## Pixel contract

The earlier plan used VCOUNT switch values 18/124 as approximate boundaries.
Inspection of the completed village frame shows top separator rows **16–18**,
gameplay **19–124**, and bottom separator rows **125–127**. The presentation
pass uses those measured pixel boundaries; it does not cover gameplay row 124
with a tan strip. The September 9 fix also aligns the underlying BG0 margin
crop with these completed rows, instead of using the VCOUNT trigger numbers.

- Only added left/right columns on rows 0–18 (0–58 while START-paused) and
  125–159 may be written by the HUD pass.
- Every pixel in the original 240x160 center remains read-only.
- The entire expanded gameplay band, including authored black boundaries,
  remains identical with the toggle on or off.
- BG0's normal HUD descriptor and the existing Mode-0 battle signature are
  prerequisites, not sufficient proof by themselves.
- Blank native HUD gutters and all six horizontal separator rows must match
  a reviewed normal/START-paused shape in the completed frame. Validation finishes before any
  pixel is written; an overlay/transition that breaks the shape is left alone.
- Samples come from palette-backed HUD pixels after scanline composition,
  blending and brightness effects. No fixed tan RGB, stale cached color or
  dominant-color guess is used. Health bars often dominate a row's histogram.
- Uniform black/white covers fail HUD authentication. The existing near-black
  cutscene cover behavior is preserved and takes precedence.
- Layer-isolation captures never get synthetic HUD pixels. The existing
  overworld/sprite/black-boundary policies are unchanged.

Game files: `src/battle_hud_borders.*`, `src/swordcraft3_display_settings.*` and
the integration in `src/adaptive_widescreen.cpp` / `src/main.cpp`.

## Validation

`swordcraft3_presentation_tests` exercises widths 241/263/284/320/383/384,
asymmetric margins, Native no-op, every protected pixel, corrupted HUD samples,
separator rejection, all-black/white covers, fade/palette/scanline transforms,
menu metadata/callbacks, persistence, invalid INI values and session overrides.
These tests do not require a ROM. They use checks that remain active under
`NDEBUG`.

Run the local private replay audit with Python 3.11+:

```powershell
python tools/validate_battle_hud_borders.py `
  --output-dir validation/battle-hud-borders-NEW-RUN
```

It requires the existing Slot 5 village save, forest frame-4980 capture,
overworld frame-33123 capture, verified private inputs and the original Alpha
executable. `--baseline-executable` can select the original unstripped capstone
binary or its verified stripped Alpha copy. The audit clones only the runtime
files it needs into a fresh ignored validation directory; private input saves
and the original package are read-only. Captures, logs and cache outputs stay
inside that directory. A used validation directory must never be uploaded.

For each case, compare Native on/off, wide on/off and the hash-verified prior
executable (only margin rows 18 and 124 may differ from capstone after the raster
fix). Check full native centers, the complete gameplay band, per-row HUD
colors, guest state, and separate BG0/BG1/BG2/OBJ passes. Native/Wide state
differences must be recorded separately: exact old/new equality at a given
width demonstrates a border-specific no-regression result, NOT that an
existing cross-width execution discrepancy is acceptable or resolved.

The runtime emits `swordcraft3_battle_hud_frame` alongside the existing
widescreen telemetry, with enabled/eligible/applied flags for the actual
completed frame. Raw route evidence remains diagnostic where dynamic-IWRAM
fallback is involved. Later battle layouts, moving cameras, unusual effects,
interactive menu behavior and live resizing still need owner play-testing.

## Verified local result (2026-09-08)

Both Japanese and English-beta executables rebuilt successfully. Four focused
CTest checks passed: presentation tests, widescreen route-auditor unit tests,
runtime monolith guard and PPU smoke tests.

English-beta executable SHA-256:
`7F4B3461E619F5DEAC97A406DFF59C34990A4830D25A6754310CFDD0F37359E9`

Japanese executable SHA-256:
`99392C154D4D864F6E3B74183A33188F2EA22B472E35AE4C9E96D5632FC72EF5`

[Final replay report](../validation/battle-hud-borders-20260908-final/report.json)
| [Village with borders](../validation/battle-hud-borders-20260908-final/village/384-on/raw/wide/composite/f_011431.png)
| [Village before](../validation/battle-hud-borders-20260908-final/village/384-off/raw/wide/composite/f_011431.png)
| [Forest with borders](../validation/battle-hud-borders-20260908-final/forest/384-on/raw/wide/composite/f_004982.png)
| [Unchanged overworld](../validation/battle-hud-borders-20260908-final/overworld/384-on/raw/wide/composite/f_033125.png)

- Village frames 11431–11435 and forest frames 4982–4986 passed at 284, 320
  and 384 pixels: full native centers identical, entire expanded gameplay band
  unchanged by the toggle, every added HUD pixel equal to its live row sample.
- Native on/off was identical. At each tested width, Off reproduced both the
  image and guest state of the hash-verified prior capstone executable exactly.
- BG0/BG1/BG2/OBJ isolation remained identical with the toggle on/off at 384.
- Overworld/NPC frames 33125–33129 remained completely identical on/off at 384,
  including the existing black transition boundaries; the native center and
  guest state also matched Native in this bounded replay.
- Forest Native-versus-Wide guest state still differs, including execution /
  IWRAM / OAM / timing components. The difference was reproduced in the old
  capstone and is unchanged by this presentation feature. It remains an open
  general widescreen audit issue, not a fully-static correctness pass. Do not
  attribute its root cause solely to widened culling without further diagnosis.

Earlier local runs are preserved: pass1 stopped on that pre-existing forest
cross-width state discrepancy; pass2 used frame 6308 as a supposed overworld
fixture, but inspection proved it was another forest battle. The final run
uses the visually verified frame-33123 overworld capture and asserts scene
family telemetry before applying each test's expectations.

The Alpha archive SHA-256 remains
`0ADD8FE8AFE1542E7D5C93174225C9AA2B86E4CE7CBB2DCAAEAF35B9A3B88D2F`.
No submodule changes, commits or GitHub pushes were made for this experiment.

## September 9 correction: gauges, pause and corner seams

The owner's F10 captures 7020 and 7960 reproduced lower HUD stripes. Native
x231 is not a blank gutter throughout the bottom HUD: hearts and the charge
gauge occupy it. The pass now validates and samples the blank x176..183 gap
on rows 128..143, then x225..231 below the gauges. This retains each row's live
composed color, including fades, without extruding gauge artwork.

A replayed START press reproduced the repeated paused HUD. Its upper separator
is on rows 56..58, not 16..18. The border pass authenticates either complete
layout before writing margins, preserving the original center and remaining
gameplay. Unrecognized overlays and transition shapes still fail closed.

The BG0 near-scenery crop previously stopped before row 124. That exposed black
or a distant plane just above the lower border, depending on the side. Both
new states contain VCOUNT events 18/124 for BG0CNT and BG0VOFS; their completed
scenery occupies rows 19..124. The game adapter now uses those pixel boundaries.
This changes only expanded BG0 margin rows 18/124, independently of the HUD
toggle. No camera, map bounds, guest memory, or shared submodule was changed.

Current English-beta SHA-256:
`179DA2F7D0CAA03BF6179C381798B6C6C0FED7902293B2CAAE98BB2B0A42B532`

Current Japanese SHA-256:
`658224AE61B760A688A4C746EDB8419681A37A816F02A4A2307056F742D0D6EB`

The pre-fix beta is preserved at
`validation/battle-margin-fixes-20260909/previous-runtime/SummonNightSwordcraftStory3RecompBeta.exe`.
The original Alpha archive is untouched. Turning HUD borders Off removes the
tan presentation, but does not undo the raster seam correction.

Reproduction tool: `tools/validate_battle_margin_fixes.py` takes a new
`--output-dir` under validation and `--previous-executable`; `--pause` replays
a START press. It checks unchanged native centers and guest state, uniform tan
lower margins, paused top margins, and limits changed rows. Original input
states are hashed before and after; runs use isolated save/config files.

All final checks passed:

- `validation/battle-margin-fixes-20260909/final-normal/report.json`: captures
  7022..7024 and 7962..7964, at widths 285 (the actual resized capture width)
  and 384. Changed pixels are confined to margin row 124 and the gauge stripes;
  native centers and guest-state traces are identical to the pre-fix build.
- `validation/battle-margin-fixes-20260909/final-pause/report.json`: START-paused
  frames 7990..7992 at 285/384; full top and bottom wings match the authentic
  background/separators. Native center and guest state remain identical.
- `validation/battle-margin-fixes-20260909/full-regression/report.json`: earlier
  village/forest cases at 284/320/384, Native, overworld and isolated layers.
  Off matches capstone except the deliberately corrected BG0 margin seams;
  guest-state equality is unchanged. The pre-existing forest Native/Wide
  discrepancy remains documented, not resolved or concealed.
- Four focused CTest checks passed; both Japanese and English-beta targets
  built successfully. Replay coverage is English-beta, not a JP playtest.

Particle popping and larger left/right scenery coverage differences are NOT
claimed fixed. A code-review lead for the next pass: the game widens OAM writer
culling past x255, but does not install `g_ws_obj_x_provider`; the shared wide
renderer defaults to signed 9-bit X. Values 256..311 can therefore decode off
the left edge at 384 width. Neither F10 state's final OAM contains such an X,
so this is not yet proven to explain the reported effect. Reproduce through a
moving edge sequence and identify the actual effect/OAM writer before changing
the adapter. Do not replace deliberate black arena/overworld boundaries with
fabricated scenery or broaden unrelated sprite-culling rules.

Later on September 9, the captured duplicate was proven to be a wrapped BG2
attack canvas, and the owner explicitly requested a matching full-width looping
distant backdrop. That continuation is now implemented and validated; see
[BATTLE_LAYER_CONTINUATION.md](BATTLE_LAYER_CONTINUATION.md). The signed-OAM-X
lead remains a separate unmodified issue. The newer background policy supersedes
the earlier finite-backdrop behavior but does not alter overworld boundaries.
