# Second area expansion — 2026-09-21

Continues [the first expansion](ADDITIONAL_AREAS_20260921.md). Current supported
scope: five authenticated field profiles and battle IDs 0, 2, 3 and 7, at 12:5.
No submodule, generated guest code, camera, collision or simulation changes.

## New source evidence

Capture session: `validation/playtest-20260921-152503-440`.

- Frame 20742: scheduler hook 08031BC8 identifies arena 7, lifecycle 4, mode 2.
  Descriptor at 08B801CC + 7*28 selects near asset 7 and far asset 39. Decoding
  the source headers through the existing archive lookup gives 384x160 for both.
  Asset addresses are 08C33ECC and 08C3915C. The descriptor includes three
  animated decoration objects; existing drawing hooks retain game ownership.
- Frame 30319: complete field layers are 632x616 (79x77 entries) with no
  placement animations. Added `field-79x77`, authenticated by all three source
  hashes. Increased fixed storage from 4,329 to 6,083 cells. The profile name is
  structural; its translated location name has not been established.

Arena 7 shares the reviewed finite 384-wide near-map policy and repeated
384-wide far-strip policy with arena 2. Both retain per-row scroll and original
HUD/effect schedules. Arenas 0 and 3 keep their prior policy unchanged.

The remaining arena header inventory is not an enablement list. IDs 1,4,5,6,8,
9,10,13 and 16 have 384x160 near/far map headers, but still need scene/lifecycle,
animation and effects checks. IDs 11/12 omit near scenery; 15 omits far scenery;
14 has a 512x256 far map. Equal map dimensions do not establish compatibility.

## Verification

Executable SHA256: `d9c9f1dc0f52fd40f1f656147feed56f9256fc37d12bdfe5f98879d9cd2783f4`.
Private evidence: `validation/area-expansion-batch2-20260921`.

- `report.json`: 210 arena samples with R selection, jump and pause/resume;
  370 field samples with input in all four directions. Repeat runs with native
  object submission match all recorded guest-region hashes and cycles exactly.
  Post-restoration ownership-warmup frames engage widescreen throughout.
- Expanded-object runs preserve cycle counts, VRAM, palettes and ordered OAM
  attributes for native-intersecting objects. Battle EWRAM is identical; field
  EWRAM changes are limited to the source-reviewed entity drawing/result members
  asserted by the validator. Additional IWRAM/OAM drawing data legitimately
  differs. This is not a claim of complete IWRAM equivalence.
- `regression/report.json`: rocky R changes (150), forest critical (90), lake
  (30) and arena 2 R changes (150) match all compared guest hashes/cycles.
- `regression-field/report.json`: prior 888x312 field (60) also matches exactly.
- Three CTests pass: battle-state gates, field-profile/animation clocks, and
  generic replay-policy state. The battle gate test rejects all other byte-sized
  arena IDs. Combined launcher `--check` passes.

The first arena-2 regression assertion rejected its unowned first restored
frame. Before and after logs contain the identical frame (completed 1, hooks 0,
active 0, wide 0, reason no-battle-owner). The validator now permits exactly
that paired startup frame for this capture; all following frames must widen.
This does not permit transient fallback during normal play.

No pixel/image assertions or screenshots were used. Source states remain
unchanged, and runs use isolated battery saves and the included TCP debugger.

## Use and remaining work

Use `Launch Custom Renderer - Overworld and Combat.bat` in the main folder.
The same settings launcher, F10 captures and separate test saves are retained.
`--previous` disables all September 21 added profiles: fields 111x39 and 79x77,
and battle IDs 2 and 7. It restores the earlier three-field/two-arena scope.
The immediately preceding executable is also preserved privately at
`validation/area-expansion-batch2-20260921/before.exe` (SHA256 `69ec5114...`).

Four captures around frames 13436–13468 show battle mode 7 in arena 3 with a
different effect layer configuration. They are not new arena IDs. This batch
does not change their effect policy; investigate them separately before claiming
all battle effects are supported. Appearance, natural scene entry/exit and long
play sessions still need user testing. Nothing has been committed or pushed.

The total number of distinct playable field areas is not yet verified. Field
archive member counts must not be presented as room counts: the renderer loads
several layer/graphics resources per field and resources can be reused.
