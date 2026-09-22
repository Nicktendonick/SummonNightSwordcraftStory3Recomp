# Reversible general-field 12:5 experiment

Follow-up: [field-tool framing correction](FIELD_TOOL_FRAMING_20260922.md) keeps
authenticated tool actions wide during their temporary movement locks, without
relaxing foreground-script cutscene framing. Its executable supersedes the build
identity recorded below; the original experiment evidence remains historical.

## Run and rollback

In the owning Documents project, double-click **Launch General Field 12x5
Test.bat**. It opens the usual settings launcher and uses the existing custom
test saves, combat support and F10 capture path. Host presentation is 384x160.

To disable general-field expansion, close it and use **Launch Custom Renderer -
Overworld and Combat.bat** normally. That launcher explicitly clears
`SWORDCRAFT3_CUSTOM_GENERAL_FIELDS`. `--previous` retains its earlier meaning:
disable the September 21 additional profiles and arenas as well.

The exact pre-change executable is preserved privately at
`validation/general-field-20260922/before.exe`. Do not confuse disabling general
maps with restoring the old pixel-based UI test: state-based field ownership is
now shared by both modes. No commit or push was performed.

## Behavior and supported scope

- Shared task-list ownership and field input/script flags establish field state.
  A stale map pointer does not authorize widening. Script-blocked periods keep
  native framing, including gaps between boxes; the previously verified chief
  ambient-event exception remains narrow.
- General mode authenticates each of three loaded base maps against its actual
  compressed ROM resource. It no longer needs a per-room hash entry for a
  compatible **nonanimated** map. Known animated profiles remain supported.
- Each layer has its own dimensions and bounded dynamically sized storage,
  removing the old 79x77-cell fixed ceiling. Decoded resources and source spans
  cannot exceed 256 KiB apiece. Synthetic tests cover larger, unequal layers.
- Immutable source decoding is cached for the current ROM/field/resource key.
  Key changes, scene ownership loss and host reset invalidate it. Full loaded
  source bytes are still compared on capture to reject mutations at the same
  pointers. This is not a cache of previously displayed scenery.
- Fast task/control checks remain per capture (and for object-hook eligibility).
  Decompression is not per frame. Tested stable scenes decode once; the menu
  ownership change invalidates and reauthenticates the source.
- The legacy BG0 pixel-visibility test was removed. Scene detection and tests
  use task/script state, source bytes, descriptors, hardware registers and
  ordered map entries, never screenshots, colors or framebuffer comparisons.
- Guest camera, collision, activation distances and simulation are unchanged.
  Native scanout is copied untouched into the center. Existing widened NPC
  body/resource/shadow safeguards and finite black-boundary composition remain.

The first generic family still requires all three present 0x4000 text-map
sources, the reviewed field display/priority schedule, no unsupported blending
or scripted animation, and coherent source-to-upload mapping. Missing layers,
different display modes, unfamiliar placement animations, overlaps and other
effects fall back to native. This is **not all-room support**, an interior-room
visibility solution, a camera clamp, or a battle expansion.

Known-profile metadata is retained for animation clocks and the chief ambient
event. A geometry match alone cannot borrow those exceptions: all three source
identities must match. An unfamiliar animated room is rejected rather than
given a lake animation clock.

## Verification

Private evidence is under `validation/general-field-20260922`:

Built executable: `build-native/Swordcraft3CustomRendererBeta.exe`, SHA256
`1cac6dda5110d02d6fad201194315d84c59f9a6a04c984052487ac1ba6e99550`.
Both the ordinary launcher and owner-folder general-field launcher pass
`--check` without starting an interactive game.

- `regression2/report.json`: 374 paired old/new frames over five field layouts,
  two script states, an arena and menu input, with native object submission.
- `expanded-objects/report.json`: the same 374-frame sequence with expanded
  NPC drawing enabled. In both sets, all recorded guest hashes and cycle counts
  match the preserved old executable. Existing field/menu/script framing also
  matches. The arena never qualifies for field rendering.
- Stable fields decode their ROM maps once. The menu case decodes twice across
  ownership loss/reentry. Both scripted samples remain narrow throughout.
- The village movement sequence legitimately starts an event at completed
  frame 15. Both builds switch to native on that frame; flags change from free
  control to 4/0x14. The first validator incorrectly demanded constant wide
  during movement; its failure was retained in `regression/`. The corrected
  test requires agreement with the old build through the event, not an ignored
  exception or broader renderer permission.
- Five targeted CTests pass: field source/cache, Python field-provenance
  (14 cases), existing lake control, animation clocks and battle state.
  Tests include corrupted links, stale ownership, malformed compression,
  same-address source mutation, resource-key changes, reset and unequal/larger
  synthetic layers. Assertions remain enabled for the new C++ test.
- `performance/report.json`: three warmed 120-frame batches per mode on the
  captured 632x616 field, host width 384, no screenshots, no extra replay, no
  phase instrumentation. Median processing time: profile mode 9.32 ms/frame,
  general mode 9.25 ms/frame. End guest-state hashes match. This small difference
  is not an FPS improvement claim; displayed pacing and audio were not tested.

Source saves were loaded read-only and stayed unchanged. Tests used isolated
battery files and the included TCP server. No 16:9 or 2:1 tests were run. The
original center was not redrawn or compared as an image.

## Remaining playtest

The actual capture collection contains only the five previously supported field
layouts. No genuinely new room is claimed as playtested. Use the new launcher to
visit a formerly narrow area, move through both edges, talk to an NPC, open/close
the menu, and enter/leave a battle. F10 captures preserve the source-state evidence
needed if the area stays narrow or a transition fails. Existing known-area tests
do not certify unseen event scripts, every menu type, a full natural field/battle
round trip, or new-room NPC resource pressure.

Relevant implementation: `src/custom_field_source.h`, `src/custom_field_scene.h`,
`src/custom_renderer.cpp`; game-specific helpers/tests stay in this repository.
No reusable engine/UI or generated guest-code changes were necessary.
