# Independent arena and effect eligibility — 2026-09-23

## Change contract

Custom-renderer experiment only, 12:5 (384x160). An unfamiliar BG2/BG3
effect format no longer invalidates otherwise verified arena scenery. The
game still needs the authenticated battle scheduler/lifecycle, supported
arena, captured Mode 0/1 BG0/BG1 descriptors, complete HUD raster schedule,
and reviewed near-map source-Y bounds. Lost ownership, forced blank, modes
without the regular scenery layers, changed scenery, or unknown arenas keep
the full native fallback. This is not universal arena/map support.

No guest instruction, camera, collision, actor activation, resource loading,
simulation timing, or save-state change. Native 240x160 scanout is copied
unchanged into the host center. No additional native replay/check is enabled.
Validation uses registers, source, memory and control flow, never pixels.

## Source inventory

`tools/audit_battle_effect_families.py` checks the 16-entry dispatcher at
080391E8/08039210, plus JP/beta equality of 21 reviewed routines. Effect kind
is owner+044E, stage owner+044F; kind IDs are **not** localized spell IDs.

| Kinds | Handler | Role seen in source |
| --- | --- | --- |
| 0 | 0803DB60 | Reset into default effect lifecycle |
| 1 | 0803A8EC | Default regular canvas lifecycle |
| 2 | 0803CA40 | Scripted OBJ and regular/affine BG2; secondary regular BG3 |
| 3–5 | 0803B5F0 | Regular casting (3/4); affine casting (5, via 0803BB88) |
| 6 | 0803AFA8 | Nonwrapping 128 affine, priority 2 |
| 7 | 0803BDC8 | Nonwrapping 256 affine critical impact |
| 8 | 0803D714 | Default cleanup branch |
| 9–12 | 0803BFFC | Nonwrapping 128 affine scale/rotation variants, including observed Dark Hole |
| 13 | 0803C2F8 | Actor/status sequence with palette tasks |
| 14 | 0803C618 | Palette-task sequence |
| 15 | 0803C78C | Actor sequence with palette tasks |

The script update at 0803CE14 uses 0803D3C8 for regular BG2/BG3 and
0803D1FC for affine BG2. 0803CF68 resets both script backgrounds. The
inventory accounts for dispatcher branches, not every script opcode,
resource, summon, localized spell name, or possible game situation.

## Policies

`src/battle_layer_policy.h` now separates arena layout from effect placement.
The old combined whitelist remains only as a historical regression helper;
it no longer controls CustomBattleScene's whole-frame eligibility.

- Existing single-copy regular effects retain their placement restriction.
- Signed regular effects require the existing authenticated script/casting
  rectangle and exact register correspondence. An unknown 512px effect is
  not repeated across the margins.
- Reviewed affine canvases use the captured per-row transform and texture
  bounds, not a text-map repeat rule. The priority-2 0386 canvas is additionally
  covered by source-derived policy; it has not been exercised as a real spell
  in the available captures.
- Unknown BG2 descriptors/window combinations remain native-only. BG3
  scripted effects remain native-only until their ownership and placement
  are separately established. This can leave an effect visibly clipped at
  the original screen edge, but does not shrink the verified arena.
- BG2 effects cannot paint the HUD wings, including affine effects that do
  not pass through the regular-text margin callback.
- No historical 'last good effect' is kept. Each completed captured row gets
  its own restriction; setup, activation and retirement cannot inherit
  another row's permission. Native blending/window semantics are untouched.

The reusable `GbaReplayViewPolicy::margin_layer_mask` lives in gbarecomp on
its existing dedicated engine branch. It is an instance-local per-row
restriction of BG0..3/OBJ outside the native viewport. It applies before
regular authored-window bypass and through the shared affine/bitmap/OBJ
eligibility gate; null keeps existing engine behavior. The game chooses
the permitted roles. It does not change guest register values.

Diagnostics append `effect_policy` (0 native-only, 1 single regular,
2 signed regular, 3 bounded affine) and `margin_layers` (19 scenery/OBJ,
23 scenery/OBJ/BG2) to the opt-in battle-window state trace.

## Verification and rollback

Evidence is private under `validation/spell-families-20260923/`.

Completed results: all 12 selected state-based CTests passed. The seven spell
cases passed 1,094 paired frames (including three independent restorations);
the eleven battle/negative cases passed 1,386 paired frames with zero recorded
guest-region/cycle differences. Total: 2,480 paired frames, not an exhaustive
spell playthrough. The launcher `Launch General Field 12x5 Test.bat --check`
resolved the current 384px build without opening a game.

Tested executable SHA256:
`3c637618d7cf60e05e88084ec10e4df052dae0dccb20b9a3419de3d8826292b7`.
`state-tests.log`, `paired/report.json` and `battle-regression/report.json`
record the results. Older Dark Hole/casting validators now distinguish the
new host-policy diagnostic fields from the unchanged captured registers.

- `source-audit.json`: all 16 entries accounted for; 21 reviewed code ranges
  identical between the local JP and beta ROMs.
- State-only unit/integration suite: independent arena/effect decisions,
  exhaustive register eligibility, one-row changes, unfamiliar and wrapping
  descriptors, BG3 enabled, forced blank, wrong scene/arena/scenery/mode,
  real immutable raster replay routing, per-row engine restriction, native
  coordinate permission, and authored-window bypass precedence.
- `tools/validate_effect_families.py`: paired isolated TCP spell replays,
  checked full IWRAM/OAM and all recorded region/cycle hashes, captured
  registers, continuous owned-wide coverage and repeated restoration.
- `tools/validate_battle_state.py`: R, jumping, pause, attack, critical,
  victory/exit and negative field cases. No live user saves or game inputs.

Synthetic unknown-descriptor/BG3 tests establish routing and fallback, not
the artistic correctness of unplayed effects. There is no claim that every
spell animates fully beyond the original viewport, no independent CPU
oracle, and no new FPS/audio measurement. The chest translation and rewind
bugs are explicitly out of scope.

The pre-change executable is preserved at
`validation/spell-families-20260923/before.exe` (SHA256
`c0afd54dfde67bc91724899c3b66da0413fd18128437f1e0473d06875de07be4`).
Do not reset the working tree: it contains earlier uncommitted field and
spell-window work. Nothing was published by this change.
