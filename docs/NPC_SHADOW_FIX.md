# Offscreen NPC shadows — 2026-09-13

The owner accepted the corrected NPC graphics, then reported missing shadows
outside the native viewport. The shadows are separate regular OBJ sprites, not
part of the body graphics. Two independent native-width gates remained:

1. `0809FD6E` checks `(npc.flags & 0x24) == 0x24` before submitting the shadow
   record at NPC + `0x44`. Supply extended visibility bit 4 to this exact read,
   subject to the existing actor rectangle, map/control and residency guards.
   Leave the actual shadow-enable bit `0x20` unchanged.
2. `sub_080091C4` replaces positions outside its native bounds with `(240,160)`.
   Opt in its right immediate at `080091CE` and left immediate at `080091E0`.
   Only widen these when LR matches one of the seven reviewed NPC shadow calls
   and R3 identifies a resident, shadow-enabled NPC's shadow record.

The game's own shadow pose, placement, graphics, palette and OAM submission are
retained. No guessed ellipse, transparency filter, or broader sprite-mode
support was added. Native gameplay visibility flags remain untouched. Existing
black-boundary occlusion and dialogue framing still apply.

All changes for this fix are game-specific and remain in the game repository.
The reusable runtime submodule needed no new changes.

## Private build and safety checks

The experimental fast build previously reused all cached guest objects. The
two new immediate sites require regenerated guest code. The lab now regenerates
under ignored `build-native/shadow-codegen` and compiles shards 007 and 024 in
place of their cached objects, leaving the owner's original cache untouched.

`tools/prepare_lab_shadow_codegen.py` verifies that the entire generated source
set matches the baseline after removing only the two intended immediate hooks.
Unexpected source changes, missing hooks, or a changed source set reject cache
reuse. A failed generation/verification must be corrected and successfully rerun
before using its outputs; do not manually bypass the check. Four unit tests
cover these cases. The normal full-generation configuration contains the same
two opt-in sites in `symbols/swordcraft3_jp.toml`.

Build log: `validation/build-npc-shadows-position.log`.

Executable SHA256:
`3174231ccecaab5129a908625dfcc976dd055295e7d3bdf2348a3e6cf40f50e3`.

## Verification

The actual localhost TCP debugger loaded private states, stepped frames,
submitted movement input and returned native/host screenshots. Five final runs
passed with unchanged original-screen pixels, matching frame-boundary cycle
counts, and unchanged input save files:

| Local evidence under validation | Width | Frames |
| --- | --- | --- |
| tcp-npc-shadows-complete-village | 384 | 60 |
| tcp-npc-shadows-complete-chief | 384 | 60 |
| tcp-npc-shadows-169 | 284 | 60 |
| tcp-npc-shadows-walk | 384 | 120 |
| tcp-npc-shadows-dialogue | 284 | 30 |

Village, chief, 16:9 and movement output screenshots were visually inspected.
The owner subsequently reported, "It looks good!" Six focused CTests also
passed, along with four codegen-guard unit tests and the launcher path check.

Updated final raw host RGB references for the same 60-frame released-input
captures described in `NPC_RESIDENCY_FIX.md`:

- Village 384: `8cbd32f152a9ddfb41cdd10552619d793eeeb434cc8499b2ec51fb61ebb0c38c`
- Chief 384: `68efe002afe9d30e4be0d3edef89bf80ec38dadfdf25e8c28cbe74085bfe50a4`
- Village 284: `4dba78beefc5da6b1c080db12653db57eabf34185f91040de8d1fc48c12441dc`

The debugger validator now also saves OAM, IO, VRAM and palettes locally for
diagnosis. Never commit these ROM-derived captures or save files.

This is short-route validation for the existing three outdoor profiles, not
proof of all-game correctness or long-session performance. The prior residency
and old-save warmup limitations still apply. Use the existing top-level
`Launch Custom Renderer Lake Test.bat`; no GitHub push or release was made.
