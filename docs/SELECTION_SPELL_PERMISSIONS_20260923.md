# Field selection and inactive spell-window permissions — 2026-09-23

## Scope and contract

Custom-renderer worktree, 384x160 (12:5) only. Two presentation permission
corrections, not a new spell-specific asset override. No guest instruction,
input lock, camera, collision, spell timing, save or resource-loading changes.
The native 240x160 scanout remains copied into the center unchanged by
construction. No duplicate native render or framebuffer comparison. Reusable
engine code was not changed in this turn; earlier dirty engine work is retained.

Evidence: `validation/spells-r-20260923/`, using all nine captures from
`validation/playtest-20260923-170024-356/`. No live saves/game inputs were used.

## Confirmed causes and corrections

### Overworld R/L tool selection

Both field captures had task owner 0200E000, flags 1000 and action callback
0809D859, substate 1, set mask 1000, clear mask 1. The earlier predicate
recognized tool strikes and object interactions but not selection animation.

080A4BEC allocates this callback. 0809D858 has states 0/1 and calls 0809D7B8
to handle repeated L/R changes while the animation is running. Completion
releases the lock; a tool strike hands off to another action. Added this
callback to `field_tool_action`, retaining authenticated field/ROM ownership,
all seven tool indices, exact masks/active flags, foreground-script exclusion
and rejection of unknown concurrent lock owners. Both field rendering and
NPC permission use this same predicate. Real dialogue remains native-framed.

### Casting and scripted spell window rejection

Frames 8915 and 9524 used WININ=5537 instead of the previously required 3F37.
Only WIN0 was enabled. Both values give WIN0 the same permissions (37);
the differing high byte belongs to disabled WIN1. Rejecting it prevented the
signed casting/script rectangle from extending into the margins.

The decoder now validates active WIN0 and outside-window permission bits,
ignoring disabled-window bytes and unused permission bits. It still requires
WIN0 alone, the reviewed mode/layer format and blend operation, authenticated
effect ownership, actor identity, dimensions, and agreement between signed
coordinates and the actual captured registers. Additional WIN1/OBJ windows,
different active permissions and unsupported owners retain native fallback.

## Remaining fire/attack complaint — not declared fixed

User identified the fire-burst-like attack as the priority. Five of the seven
battle captures already had accepted signed windows and extended BG2 sampling
before these changes: frames 16119, 20088, 26132, 26177 and 26205. Their effect
policies are unchanged by this patch. The first pair uses script ID 14; the
last three use script ID 15. These script IDs are not being treated as an
authoritative localized spell-name mapping.

Source review of 08004D6C, 0800549C and 080059A0 shows that these nonstreamed
512x256 canvases use complete placement updates, not the 31-column streaming
routine at 080057E4. `audit_reported_spell_maps.py` verified all placements
of the six scripted captures against complete ROM animation frames, including
horizontal flip and tile/palette offsets. Each capture has a coherent matching
displayed phase. Two captures have a newer RAM work map than displayed VRAM;
the captured VRAM phase still matches source. No historical map substitution
was added.

This rules out missing uploaded map entries in those snapshots; it does not
prove artistic correctness, sprite-effect completeness, or every transient
frame. Do not claim the fire attack is fixed. A reproduction identifying the
precise anomalous attack/transition is still needed. Do not broaden window or
effect permissions solely because a user-visible symptom remains unexplained.

## Verification

- 12 selected state-only CTests passed. Field-action tests cover every tool,
  slot and reviewed selection state, unknown states, foreground scripts and
  concurrent arbitrary-event ownership. Spell-window tests vary all 256
  disabled-window high bytes in casting and script paths; existing active
  permission/extra-window/ownership/retirement negatives still pass.
- `validate_selection_spell_permissions.py`: nine 60-frame captures plus two
  243-frame R/L cycling and tool-use routes, 1,026 paired frames. Complete
  recorded guest hashes, cycles and metadata are equal. Captured registers
  are unchanged; only the declared host permissions/framing change.
- In the two short field captures, 10 and 9 narrowed frames become wide.
  Repeated selection routes recover 162 and 161 formerly narrowed frames.
- Casting/script cases gain 6,400/9,440 valid window rows and
  53,040/403,560 margin source candidates. These are permission counts,
  **not opaque pixel counts** or independent rendering verification.
- Five battle/negative routes add 516 paired frames: R, jumping, critical
  impact, native lake case and field case. Zero recorded guest differences.
- Total: **1,542 paired frames**. JP/beta equality checked for the reviewed
  selection and animation uploader routines. All original captures unchanged.
- `Launch General Field 12x5 Test.bat --check` resolves the 384px executable
  without launching the game.

Executable SHA256:
`1b952a23efa5c8c5083a2e30c43da01dbc91775a9d420cd37c1a92b66d119c27`.

Rollback executable: `validation/spells-r-20260923/before.exe`, SHA256
`3c637618d7cf60e05e88084ec10e4df052dae0dccb20b9a3419de3d8826292b7`.
Do not reset the worktree; it contains previous uncommitted changes.
Nothing committed or pushed by this turn. No FPS/audio/CPU-oracle claims.
