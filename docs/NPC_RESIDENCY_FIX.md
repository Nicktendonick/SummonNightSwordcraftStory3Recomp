# Offscreen NPC graphics-residency correction — 2026-09-13

The subsequent [NPC shadow correction](NPC_SHADOW_FIX.md) preserves this fix
and supersedes the executable and visual-reference hashes below.

The initial extended draw-list implementation was incomplete. It allowed NPC
sprites outside the native viewport to draw, but `sub_0809F008` still freed their
graphics and palette resources when the original visibility bit was clear.
Residual graphics could look correct briefly; reused allocations produced the
striped and incorrectly colored NPCs reported in the user's latest captures.
Native-center pixel comparisons alone did not detect this regression.

## Correction and scope

Game-specific changes are in `src/custom_renderer.cpp` and
`src/custom_field_objects.h`. No additional engine change was needed for this
correction.

- Extend the graphics-residency visibility read at `0809F01E` consistently with
  the NPC draw read at `0809FD38`.
- Require the reviewed allocator caller (`LR = 0809FC7B`), expected register
  member address, valid stack range, and native viewport rectangle. The nested
  allocator's parent stack is current SP + 20 bytes; its actor rectangle is at
  parent SP + `0x10`.
- Keep existing map/control restrictions and widened actor intersection checks.
  Do not change the shared rectangle test or stored gameplay visibility flags.
- Let the game's own allocator retain/load graphics and palettes. Before an
  additional offscreen allocation, check the 16 resource records, 10 palette
  records, and contiguous graphics blocks. Validate existing resource handles;
  refuse a draw with an unallocated `FFFF` handle.
- Conservatively decline extra sprites when capacity is unavailable. This may
  omit an offscreen NPC in crowded scenes rather than corrupt graphics.

The supported scope remains regular sprites in the three authenticated outdoor
profiles, at host widths up to 384. Battles and unsupported maps remain native.
`SWORDCRAFT3_CUSTOM_OBJECTS=0` restores background-only extension for comparison.

## Validation

Corrected executable: `build-native/Swordcraft3CustomRendererBeta.exe`

SHA256: `be4338dd115738cdb75e5250a9537b88602f8087d2541ea3b1edd77cd7152b87`

Build log: `validation/build-npc-residency-guard.log`.

All runs used the included TCP debugger for state loading, input, stepping,
native and host screenshots, and state hashes, with isolated battery paths.
The user's input states were unchanged. Evidence remains local under ignored
`validation/`; do not commit captures, saves, or ROM-derived images.

| Evidence directory under validation | Test | Result |
| --- | --- | --- |
| tcp-npc-residency-visual-village | Latest village capture, 60 frames at 384 | Native pixels/cycles match; reviewed full-host final RGB hash matches |
| tcp-npc-residency-visual-chief | Latest chief capture, 60 frames at 384 | Native pixels/cycles match; reviewed full-host final RGB hash matches |
| tcp-npc-residency-walk-right | Village, right input, 240 frames at 384 | Native pixels/cycles match; final screenshot enters correctly native-framed dialogue |
| tcp-npc-residency-dialogue | Prior chief dialogue, 30 frames at 284 | No image changes; all compared memory-region hashes and cycles match |

Six focused CTests passed: `swordcraft3_lake_animation_tests`,
`swordcraft3_lake_control_tests`, `runtime_monolith_guard`, `gba_native_capture`,
`gba_host_presentation`, and `ppu_smoke_tests`. New capacity tests cover free,
fragmented, and exhausted graphics pools; full palette/resource pools; existing
resident resources; and invalid resource handles. Launcher CheckOnly at 384
also passed.

### Reproducible visual references

Source directory: `validation/playtest-20260912-201201-437`.

- Village: `frame-0000021218-1789258403285/state.gbas`, source SHA256
  `51f9c0343b4722d9a1a422cc299cee255517b3277903f57d839dc7747d9c05e4`.
  After 60 released-input frames at 384, final raw host RGB SHA256:
  `ab23694b6207a522dc0c395a42d3969eaacc0faafee218a485953ae4311260cc`.
- Chief: `frame-0000017658-1789258368447/state.gbas`, source SHA256
  `e3155ab9336755162fa4808356a146a35c3b548638b187679f4048942e344b9a`.
  After 60 released-input frames at 384, final raw host RGB SHA256:
  `3496a84655f6da5852fce0a804b56003595bc83b408a44558b4e4b24af924ad1`.

`tools/validate_field_objects_tcp.py --expected-last-host` asserts this
offscreen-inclusive final image reference in addition to native-center parity.
These references were visually inspected, not taken from the defective build.

## Remaining limits

Old broken-build save states can contain stale OAM already. The first restored
frames may still show that data before fresh guest allocation/drawing completes.
This is not a claim that every restored frame or every entity type is corrected.

Graphics/palette resource bookkeeping now legitimately differs between enabled
and background-only runs. The validator's older `final_ewram_non_draw_differences`
classification does not recognize all those resource fields; it is diagnostic,
not an assertion of gameplay changes or full guest-state equivalence.

Final-image references and short routes do not establish correctness throughout
the game, long-session allocation behavior, or live frame pacing/audio quality.
Further owner playtesting is still required. No release or push is part of this
local correction.
