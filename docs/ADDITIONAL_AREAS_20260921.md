# Additional field and battle support — 2026-09-21

Historical first-batch record; see [batch 2](ADDITIONAL_AREAS_BATCH2_20260921.md)
for the newer executable, supported scope and cumulative rollback switch.

Game checkout: `experiment/custom-combat-renderer-20260913`, based on `de3025a`.
All changes here are game-specific; no engine/UI submodule changes are needed.

## Captures and causes

Local capture session: `validation/playtest-20260921-115042-843`.
Both reports have host width 384 and guest width 240, as intended.

- Frame 3773: battle lifecycle 0, field input flags 1, three complete 888x312
  source maps (111x39 entries), no placement animations. This was an unsupported
  field profile, not a dialogue fallback. Its 4,329 cells also exceed the prior
  3,200-cell storage. Added a profile authenticated by all three source-map SHA1s,
  and increased bounded storage to 4,329 cells. `field-111x39` is a structural
  identifier, not a claim about the area's translated name.
- Frame 9350: battle scheduler `08031BC8` reports arena 2, phase 4, mode 2,
  variant 0. The prior allowlist accepted only arena 0 and 3. Added arena 2,
  preserving hook-based ownership across R, jump and pause states.

Source save SHA256s respectively:
`e6b6c2cc4baf2b87ac5121ef7249113bedbeca9d125bbb48ccf0a53c3e978b04`
and `cd4ff694ec6d17724ba3a7e09453f84193c99d0edf262ce8759b1f59f026f672`.

## Arena source and bounds

`sub_08031420` indexes the ROM descriptor at `08B801CC + arena*28`.
For arena 2, map asset 2 (near) and 34 (far) both declare 384x160 source
dimensions. The map allocation is 512 pixels wide; its padding is not more
authored terrain. The new near policy stops at 384, while the far policy repeats
the 384-wide backdrop strip with its own captured scroll. No reflection, camera
movement changes, collision changes, new terrain or gameplay activation edits.
Existing arenas retain their previously accepted bounds/policies.

The source loader uses archive lookup `08001D3C/08001D78`, map header decoding
`08001DC4`, and raster scheduler/scroll builder `08031BC8/08031C88`. Arena 2 also
declares six animated decoration objects; the existing draw-only expansion
handles their offscreen submissions. Their guest animation state remains owned
by the game.

### Arena count research

There are 17 distinct configured asset sets at IDs 0 through 16, followed by four
identical fallback-style descriptors (slots 17 through 20) before the decoration
position records at `08B80418`. This is not proof of 21 unique playable arenas or
of which entries normal gameplay reaches. Some configurations omit the near or
far plane. Do not enable these other IDs just because they share an allocation.

## State-only validation

No screenshots, image hashes or framebuffer comparisons were requested or used
as assertions. Original states are unmodified; runs use isolated battery saves,
the included TCP server, input keys and frame stepping at 384x160 only.

Build SHA256: `69ec511401279ec0c9c9552831195103c3892a998dcb519fd5e6ff148f4b98ac`.

Private evidence: `validation/area-expansion-20260921/`.

- `report.json`: 210 arena frames through R selection, airborne motion and
  pause/resume; 230 field frames moving left and back right. Both were also run
  with native object submission. Every frame after the first restored ownership
  warmup is wide. Native-object runs match all guest memory hashes and timing.
- Expanded-object runs preserve timing, VRAM, palettes, and the ordered OAM
  attribute list for objects intersecting the native viewport. Battle EWRAM is
  entirely identical. Field EWRAM changes are confined to positioned draw-record
  and submission-result members of the existing entity array; its world
  coordinates, animation state, flags and other EWRAM remain identical.
- IWRAM/OAM differ with additional submissions, as expected. IWRAM differences
  were inspected in draw lists, software OAM, submission scratch and stack
  storage; the new validator does **not** claim byte-for-byte IWRAM equivalence
  or independently prove every scratch/stack difference harmless. Native-object
  comparison supplies the separate rendering-only equivalence check.
- `regression/report.json`: prior rocky R-cycle (150), forest critical effect
  (90), and lake negative battle-ownership (30) all preserve every compared
  guest-memory hash and timing against the accepted executable.
- CTests passed: `swordcraft3_battle_state_tests`,
  `swordcraft3_lake_animation_tests`, `gba_replay_policy_state`.
- Combined launcher `--check` passed without starting a user game.

Tools: `inspect_scene_state.py`, `probe_battle_state.py --objects native`, and
`validate_area_expansion_state.py --root validation/area-expansion-20260921`.
The validator authenticates the saved baseline binary when its former build
path has been replaced; it never accepts a changed executable hash.

## Launch, rollback and limits

Use the main project's `Launch Custom Renderer - Overworld and Combat.bat`.
It opens settings and uses the same separate test saves and F10 capture path.
`--previous` disables only the new field profile and arena 2, leaving the prior
areas enabled. Direct users of the underlying helper can set
`SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS=0` for the same comparison.
The accepted executable is preserved at
`validation/area-expansion-20260921/before.exe` (original SHA256 `37f9f663...`).

This expands support to four authenticated field maps and three battle IDs,
not every map in the game. Finite map edges, dialogue framing and unsupported
scene fallbacks remain. Human playtesting is still needed for appearance,
natural entry/exit, longer sessions and additional attacks/skills. This work
has not been committed, pushed or released.
