# Bow activation flash: confirmed field-framing handoff gap

Diagnosis only. No renderer/game code changed, no rebuild or publication.

Follow-up: the diagnosed gap was corrected and tested on September 24; see
[the fix record](BOW_HANDOFF_FIX_20260924.md). The diagnosis and old-build
reproduction below remain historical evidence.

## Identity and reproduction

Owner capture session: `validation/playtest-20260923-234600-931`, frames
21165, 21273 and 21376. All three contain saved game state and report host width
384. The reproduced executable SHA-256 is
`c29226f597d3a5fb0e4480f0db4cc43dc3ec348c676b6c9bcf5e4bbc0024215b`,
game checkpoint `a3a082f3`, full-viewport experiment with general fields and
expanded objects enabled. The audit report remains an unrelated uncommitted file.

Used `tools/probe_battle_state.py` and its isolated TCP process/save, never the
owner's live game. Private evidence: `validation/bow-activation-audit-20260923/`.
Original captures were hash-checked unchanged by each probe. No screenshots,
pixel comparisons or framebuffer assertions were requested.

## Confirmed cause

The field remains owned throughout. Bow tool index is 6. At the end of the draw
animation, callback `0809DA99` retires and queues the projectile; callback
`0809E3ED` is installed on the following update. Between them, movement remains
locked (`field.flags=1000`) but the action pool has no active record.

`src/custom_field_action.h::field_tool_action` requires a currently active,
recognized action and returns `found`. It recognizes both callbacks, but not
the pending projectile between them. `CustomFieldScene` therefore declines with
`lake-player-control` for one frame, then widens again. The reason label is
historical: this captured map is classified `general-field`, not necessarily lake.

This is the host framing predicate rejecting a legitimate game transition, not
missing bow graphics, a full-combat composition failure, or evidence of a new
foreground cutscene.

## Source and state correspondence

- `0809DA98`: when tool=6 and the initial animation completes, queues bit `0100`
  at field root + `1ED0` (`1EB8+18`) and returns action state zero.
- `080A4724`: consumes the request when `(word & 0700) == 0100`, clears bit
  `0100`, calls `0809E0D4` and `080A4D08`.
- `080A4D08`: allocates callback `0809E3ED` with the reviewed `1000/1` lock masks.
- In the 12-frame memory trace, request word is `0002` during initial animation,
  `0102` on the no-active-action frame, and `0203` during projectile continuation.
  Field ownership persists, tool stays 6, and flags stay `1000` at the handoff
  (foreground-script bit 4 is clear).
- Those three routine byte ranges (`0809DA98..0809DB30`,
  `080A4724..080A475C`, `080A4D08..080A4D48`) match in the local JP and beta ROMs.
  Reference source is the corresponding routines in
  `references/csm3/asm/code_small_structures.s` in the owning project.

## Observed results

- First capture, 180-frame route: neutral 45, A 15, neutral 60, A 15, neutral 45.
  The restored in-progress shot and both new shots each produce one rejected
  completed frame: 5, 67, 142 (recorded guest frames 21169, 21231, 21306).
  Other 177 completed frames stay wide.
- Separate 12-frame first-capture replay confirms the pending request word.
- Second capture, 50 neutral frames: one rejected frame, completed 15.
- Third capture, 50 neutral frames: one rejected frame, completed 3.

292 frame records total, with overlapping/repeated initial-shot coverage;
not 292 independent gameplay situations. The complete-frame combat switch is
enabled but does not own these field frames. No original saves were changed.

## Proposed correction, not yet implemented

Recognize the authenticated pending-bow request as a field-tool presentation
state, after retaining current ROM/field/source, tool, foreground-script and
conflicting-action guards. Do not broadly allow an empty action pool, relax all
movement locks, add a one-frame timer, or keep a last-good image.

Use the same predicate for field framing and NPC presentation eligibility.
Regression checks should cover each captured handoff, repeated shots, both
neighboring bow phases, absent/conflicting pending bits, other tools, unknown
lock owners, foreground dialogue and restore. This should be a game-repository
permission change, not a change to bow timing, control flags or engine rendering.
