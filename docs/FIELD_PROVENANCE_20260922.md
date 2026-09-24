# Shared field ownership and authored-map provenance

Follow-up: [the reversible general-field renderer experiment](GENERAL_FIELDS_20260922.md)
now connects these findings to the renderer. The diagnostic-only checkpoint
below is retained as its evidence record.

## Outcome and scope

There is a shared field lifecycle; field identity does not need to start with a
list of room names or map dimensions. This checkpoint establishes a read-only,
source-backed diagnostic for it. **It does not enable new rooms or replace the
current field renderer.** The existing five-profile build and 12:5 target stay
unchanged. No ROM, BIOS, save, generated code, engine or launcher was modified.

Worktree: `experiments/custom-renderer`, branch
`experiment/custom-combat-renderer-20260913`, base `de3025a` plus the existing
uncommitted area-expansion work. That work was preserved.

## Source facts

Read-only Ghidra queries were checked against `references/csm3/asm/` and exact
Japanese/beta ROM bytes. Pseudocode alone is not treated as proof.

| Routine/state | Meaning established by its reader/writer |
| --- | --- |
| `0801978C` | Initializes the task pool pointer at `0300699C` to `02000800`. |
| `08019734` | Allocates one of 16 records, each 32 bytes. |
| `080197B8`, `08019898` | Set callback at record +8 and link the task into the list. |
| `08019688` | Walks the list at pool +0x200; dispatches when flags & 0x8800 equals 0x8000. Handles pending replacement and retirement afterward. |
| `080A6974` | Registers Thumb callback `08093995` with priority 10000 and sets field script bit 4. |
| `08093994` | Shared field update. Input dispatch requires bit 0 set and bit 0x1000 clear. Field bit 4 controls the foreground script. |
| `080A6A30` | Clears field bit 4 and sets bit 0 to return control. |
| `03006B54` | Pointer to field structure; map resource records are at +0x4E0, stride 0x2C. |
| `08094A4C` | Loads three field layers from archive selection (1,2), using each record's map asset at +0x1C. |
| `08001D3C`, `08001D78` | Archive lookup: eight-byte entries, offsets in units of 16 bytes. The root-pointer table is RAM at `03002970`, not a ROM table. |
| `08001DC4`, `08005CF4` | Read map dimensions/format and source offset, then install the loaded background descriptor at `03002A20 + bg*0x34`. |

Checked byte-identical JP/beta half-open ROM-offset ranges:
`1D3C..1DDC`, `5CF4..5D6C`, `19688..198FC`, `93994..93AC4`,
`94A4C..94CC8`, `A6974..A6A4C`. The audit CLI accepts only the two recorded
full-ROM SHA256 identities. This does not establish compatibility with future
translation revisions.

The diagnostic requires one live, linked field callback, valid backlinks and
count, a bounded field pointer, and no pending callback replacement or retirement.
The last two conditions are conservative: the scheduler may still dispatch the
old callback during that tick, but the diagnostic does not authorize the pending
transition. An old field pointer by itself is insufficient.

Ownership and player control are separate. The mask `(field_flags & 0x1005)==1`
means free-control candidate; it does not prove every field effect is supported.
Scripted states can remain field-owned across dialogue gaps. The chief's existing
ambient-event exception is intentionally not generalized by this tool.

## Source-map authentication

For each owned field, the audit resolves its three actual map assets, decompresses
them with bounded LZ77, checks the reviewed 0x4000 map format, and compares the
complete row-major 16-bit map entries against the loaded EWRAM source. Dimensions
must match too, but dimensions alone never establish identity. No tile images,
palette colors, screenshots, framebuffer comparisons or image recognition are
used as evidence.

This comparison authenticates the **base map**, not animation phase, tile/palette
residency, room visibility, blend/window policy or camera behavior. No room count
can be inferred from these archive member IDs. `render_authorized` is deliberately
false in every audit record.

## Verification

Private evidence: `validation/field-provenance-20260922/`.

- `all-captures.json`: 32 saved captures; 15 field-owned, 13 free-control and
  two script-blocked. All 15 owned fields match all three ROM-derived base maps.
  No malformed task lists in this collection. The other 17 have no qualifying
  field owner; this is not a claim that every one is a battle.
- These samples cover five distinct layer sets, all already known profiles:
  lake assets 49/50/51 (360x320), chief 22/23/24 (512x320), village 29/30/31
  (512x400), field 350/351/352 (888x312), field 364/365/366 (632x616).
- `live-verification.json`: 36 field-movement frames, 72 script-blocked frames
  with two A presses, and 24 arena frames with R input. State ownership/control
  remains appropriate throughout. Full source checks pass at both field and
  scripted-run endpoints. Battle endpoints do not decode stale field buffers.
- Matched baseline runs without `--field-audit` have identical recorded guest
  hashes and cycle counts for all 132 frames. Native object submission was used
  in both runs to isolate diagnostics. Source save hashes remain unchanged.
- Fourteen synthetic unit tests pass, including stale/unlinked field records,
  blocked control, suspended/replaced/retired/duplicate tasks, bad pointers,
  cycles, malformed compressed data and same-sized but wrong source maps.
- Executable unchanged: SHA256
  `d9c9f1dc0f52fd40f1f656147feed56f9256fc37d12bdfe5f98879d9cd2783f4`.

The TCP tests use only 384x160 host presentation, isolated battery saves and no
image requests. They do not certify a full natural field-to-battle round trip,
an entire conversation ending, unseen rooms, appearance or performance.

## Durable tools

- `tools/audit_field_provenance.py`: offline state/ROM audit; metadata reports
  must remain under private validation and existing reports are not overwritten.
- `tools/probe_battle_state.py --field-audit`: records task/control state every
  stepped frame and full map provenance at two endpoints. Expensive source
  decoding is not added to the game loop.
- `tools/validate_field_provenance.py`: paired guest-state verification.
- `tests/test_field_provenance.py`: synthetic parser/state tests, also registered
  as `swordcraft3_field_provenance_unit` for the next CMake configuration. The
  tests were run directly; no new game executable was built.

## Next implementation boundary

1. Promote the verified task/lifecycle reader into game-owned renderer state.
   Observe scene/resource changes, invalidate on restore/teardown, and keep
   full-event native framing until control returns. Authentication work should
   occur on ownership/resource changes, not repeatedly decompress maps.
2. Replace the fixed profile hashes with bounded source provenance for compatible
   map families. Support per-layer geometry and dynamic capacity; do not simply
   remove the allowlist or increase one fixed array.
3. Replace the legacy BG0 pixel-visibility test in `CustomFieldScene::eligible`
   with proven game-state/UI ownership. This old code is still present; the new
   audit does not add or rely on it for classification.
4. Handle placement durations/counts, scripted animations, disabled/missing
   layers, streaming, priority/window/blend effects and finite room boundaries
   through their actual descriptors. Keep unknown families on native fallback.
5. Test a newly encountered compatible room, animated and unanimated families,
   full dialogue and menu transitions, field/battle return and save restoration.
   Then offer a reversible general-field experiment through the combined launcher.

Installing Ghidra and proving ownership do not by themselves establish that all
rooms can be widened correctly. This checkpoint removes the need to guess the
shared lifecycle and provides repeatable evidence for the implementation.
