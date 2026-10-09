# Optional two-sided battle framing

Game checkout: `experiments/full-viewport-renderer`. Default **Off/current**.
Launcher Mods and Esc Graphics share `Settings/battle-camera.ini`.
No runtime/UI submodule modifications are needed; existing game-owned menu
entries/callbacks are retained when appending the new setting.

## Contract

This is host framing, **not a new perspective camera**. It translates all
captured gameplay layers, OBJ, signed spell/window coordinates and affine
sampling together. Original parallax/perspective still run unchanged. HUD rows
keep their original centered anchor. No actor, collision, AI or guest camera
coordinate is written. Additional offscreen OBJ draw submissions are allowed.

The original camera update at `08031810` clamps its logical position to
`[0, descriptor.width - 256]`. Each native view is 240 columns; the logical
union is `descriptor.width - 16`. The captured scheduler epoch supplies camera
position and lock mode; drawing never reads next-frame live camera state.
Camera update code and descriptor table hashes authenticate the interpretation
once per ROM. Japanese and released English 1.0.5.f share those exact spans.

For host width W, span S and captured camera C:

- shown = min(W, S); pad = (W - shown) / 2
- origin = clamp(C - (shown - 240) / 2, 0, S - shown)
- gameplay native anchor = pad + C - origin
- gameplay columns outside [pad, pad + shown) receive border + shadow decoration,
  not generated scenery

Reviewed arena IDs 0, 2, 3 and 7 have descriptor width 384. At host width 384,
shown=368, pad=8 and the logical origin remains zero. At width284 the origin
ranges 0..84; at width240 this feature is bypassed entirely. This constrains
logical framing, not a promise that every perspective-projected terrain point
or unseen effect has been independently certified.

The game retains its original camera-relative object submission coordinates.
The bounded compositor unwraps the 9-bit OAM X relative to its row anchor,
not the former fixed center. Draw-list cutoffs follow the asymmetric view;
they never widen both sides by the maximum shift, which would alias OAM X.
actors are not spawned/activated by this change. HUD rows keep the old unwrap.
No generated sources are edited.

## Fallback and transitions

The border + shadow treatment is confined to existing side padding in eligible
scenery rows. At 384 columns each 8-column pad contains four walnut matte
columns, one muted gold trim column and a three-column recessed shadow ramp.
Left and right are mirrored. No shadow overlaps terrain or OBJ, no extra source
tiles are invented, and HUD rows are not decorated. Views without padding keep
their full scenery width. Forced-blank rows bypass decoration. This is a
presentation treatment, not a fix for an interior scenery-layer gap.

The framing preference controls this treatment too: Current retains its old
appearance, Bounded uses border + shadow. There are no new shader passes,
textures, heap allocations or guest-state changes. `BattleEdgeBand` layout and
composition dispatch counters support state-based checks without image tests.

Existing arena ownership, raster-layout, near-row and compositor gates stay in
force. Unsupported arenas use native fallback. Native width, field maps,
locked/special camera modes and camera positions outside the normal range use
the current view. No new broad support claim is made for bosses or spells.
Changing the preference, screen width, loading or rewinding invalidates retained
host frames. The next completed eligible raster uses the new setting.

## Reproduction and evidence

`tests/battle_camera_test.cpp`: exhaustive camera positions at all three widths,
both boundaries, narrow borders, default/off, invalid ranges, authentication
failure and portable preference persistence/rejection.
`tests/combat_frame_renderer_test.cpp`: immutable input and shifted source
callback coordinates, with HUD anchors unchanged; no output pixels inspected.
`tools/validate_battle_camera.py`: supplied private release-era battle fixtures,
input-driven movement to both camera limits, Guard, jump, pause/resume, restore;
compares exact player/gameplay state with Current and checks fallback hashes.
Private reports live below `validation/battle-camera-*` and must not be committed.

This is opt-in pending the user's visual/play-feel review. State/source tests
do not certify the aesthetic result or every enemy/weapon/spell combination.

Final local verification, September 30:

- 23 selected state/source/settings/UI regression tests passed.
- Final matrix `validation/battle-camera-1790806762302424000`: 30 cases,
  17,640 input-checked frames plus restoration checks. Both camera limits reached
  for arena IDs 0/2/3/7 at 284 and 384 columns. Exact player/gameplay records
  matched Current; native-width and unsupported-arena state hashes matched too.
- Actual Japanese and release-English camera authentication passed; deliberate
  code/table corruption was rejected. Language routing rejected old English beta.
- Final package `validation/release105-1790806777097010600`: released-patch
  import, Japanese/English handoff, cold boot, reset, graphics settings and
  launcher camera On/Off persistence passed. The existing source-ROM plus new
  patch selection was tested through the launcher before installation.
- Installed into `release/Portable Beta`. Only two engines, README, patch
  selection settings and a new named BPS changed. Whole-folder hashes verified
  that all existing saves, old inputs, controls and other files stayed intact.
  Rollback: `release/Portable Beta Before English 1.0.5f 20260930-222720-758d`.
  No publication, commit or push was performed.

## Released English packaging

### Border + shadow update (September 30)

- The 23 selected state/source/settings/UI regressions passed, including new
  symmetric edge-band layout, tiny/odd padding, HUD exclusion, native/16:9
  exclusion and forced-blank dispatch cases. No pixel assertions were used.
- Private package `validation/release105-1790812466736478800` passed the actual
  launcher patch import/toggle/persistence, both language handoffs, cold boot,
  reset and graphics smoke checks.
- `BORDER.json` records eight paired old/new input runs, 588 checked frames
  per run plus restoration checks: four supported arenas at 384 columns,
  an unsupported arena, native 240, 284, and Current/off at 384. Every recorded
  gameplay value and guest-memory/cycle hash matched the installed previous
  engine. Composition counters confirmed 16 decorated columns per eligible row
  in the padded cases and none in the negative cases. Native scanout bypasses
  the compositor; its negative check uses the state hook, not missing pixels.
- An initial test-script assumption incorrectly required compositor diagnostics
  in native mode; correcting the diagnostic expectation required no game change.
  The complete paired matrix was rerun successfully; see `BORDER.json` for the
  final run directory.
- Installed only the two engines and README into `release/Portable Beta`.
  Whole-folder hashes verified settings, saves, inputs and all other files were
  unchanged. Rollback files are in `release/Portable Beta Before Border Shadow
  UTC 20261001-000542-c566` (the folder stamp is UTC).
- This is appearance-only work; it does not certify/fix an interior layer gap.
  No GitHub publication or pixel-based gameplay verification was performed.

The current portable recipe selects `Swordcraft3Translation106` and
`Swordcraft3Japanese106`, generated for the correct instruction corpora. The
1.0.5.f installation records above are historical. See
[Follow + edge stops](FOLLOW_EDGE_CAMERA.md) for the newer separate test. It
does not ship the historical beta engine. Internal installed engine names remain
unchanged to keep the existing portable starter working. Build the pair with
the verified local `SWORDCRAFT3_RELEASE106_ROM` and
`SWORDCRAFT3_PORTABLE_JAPANESE=ON`. Never commit ROMs or generated guest sources.

English 1.0.6.f uses new `english-1.0.6f` battery/state names. Old beta saves
are neither overwritten nor automatically imported. Japanese remains available
without translation. Historical beta source targets remain for private
regression work only, not release packaging.
