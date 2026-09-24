# Flare Burn movement dropout: confirmed metadata/display timing mismatch

Diagnosis only. No renderer/gameplay changes, rebuild, commit or push in this
pass. Existing dirty work is preserved. Custom-renderer worktree, 384x160.

## Reproduction

User identified Flare Burn and left/right movement as the trigger. New session
`validation/playtest-20260923-194823-776` contains script-kind 2, stage 1,
script ID 14 in frames 5715 and 5750. The source capture used for replay was
`frame-0000005715-1790207395571/state.gbas`.

Private state-only TCP evidence: `validation/flare-burn-motion-20260923/`.
Each run restores that unchanged capture into an isolated process/save:

| Run | KEYINPUT sequence | Active spell frames | Frames rejecting BG2 margins |
| --- | --- | ---: | ---: |
| neutral | 1023:100 | 89 | 0 |
| left | 991:65,1023:35 | 89 | 17 |
| right | 1007:65,1023:35 | 89 | 8 |
| turns | 991:25,1007:25,991:25,1007:25 | 89 | 24 |

400 stepped frames total. The counts use authenticated effect-active raster
rows 40..124, not pixel comparisons. The whole arena has zero owned-wide
dropouts in all four runs. Setup/restoration and nine BG2-disabled teardown
frames are not counted as missing active effects. Tested executable SHA256:
`1b952a23efa5c8c5083a2e30c43da01dbc91775a9d420cd37c1a92b66d119c27`.

## Exact failure

`CustomBattleScene::capture_spell_window` combines the current raster row's IO
with live guest IWRAM/EWRAM. `read_battle_spell_window` reads the script's
signed X at 0200323E and requires its low nine bits to equal displayed BG2HOFS.
Those values need not describe the same update epoch while the game is moving
the effect. The game has already calculated the next position, while the
current raster still owns the previous display registers.

Example, left run completed frame 3:

- Displayed BG2HOFS remains 312 and WIN0H remains C8FF.
- At row 0 the signed rectangle is accepted: [200,712).
- From row 18 it is rejected, with the same displayed scroll and window.
- Endpoint guest descriptor X and queued shadow BG2HOFS are now 313.
- Next completed frame displays 313/C7FF and the rectangle is accepted again.

All rejected movement frames have exactly this one-pixel offset:
17 left frames are +1; 8 right frames are -1; alternating directions has
13 +1 and 11 -1. The source actor facing and map flip remain set, Y still
matches displayed VOFS, effect kind/stage/script remain 2/1/14, and the
pending shadow scroll equals the newer descriptor. In 14/17 left, 7/8 right
and 17/24 alternating cases, the acceptance changes within the same raster
frame without a change to displayed HOFS/WIN0H.

Source corroboration:

- 0803D3C8 calculates the moving regular effect position, stores descriptor
  X/Y, and calls 08005E18.
- 08005E18 updates the RAM scroll shadow at 03002D08/0A and BG descriptor;
  it does not directly publish those values to the currently displayed IO.
- 0803D5A4 computes the signed window. 0800493C writes clamped window values
  into the RAM display shadow rooted at 03002990.
- The host compares live metadata against captured display IO in
  `src/custom_battle_spell_window.h` (the script scroll equality check).

Once that check rejects, the 4305 BG2 canvas is classified `native_only` and
the margin-layer mask becomes 19 instead of 23. Both margin effect sample
counts become zero. The native center still displays the correct flame,
creating the apparent GBA-edge clipping/popping. Scenery stays wide.

## Why the earlier tests missed it

Earlier neutral replays proved complete uploaded animation maps and coherent
VRAM phases. That remains true. They did not exercise an input-driven position
update while the effect remained active. Static map completeness and guest
hash equality cannot establish metadata/display epoch coherence.

## Recommended correction, not implemented here

Keep effect identity, signed placement/facing and display registers associated
with the same committed display epoch. Capture/latch authenticated metadata
at the verified publication boundary, or derive the displayed placement from
captured registers with authenticated unwrapped bounds. Preserve the existing
unknown-owner, transition, teardown and native-center protections.

Do not simply remove the guard, allow an arbitrary positional tolerance, keep
the last accepted effect forever, or add a Flare-Burn-name exception. This is a
shared timing flaw; other moving effects using this decoder may encounter it.
Use these four motion/control cases as regression evidence for the eventual
fix, plus ownership changes, retirement, both facing directions and restore.
