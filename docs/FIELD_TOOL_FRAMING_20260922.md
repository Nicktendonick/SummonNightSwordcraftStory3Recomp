# Keep field tools in widescreen without changing cutscene state

## Run and behavior

Close/restart the game with the owner-folder **Launch General Field 12x5
Test.bat**. The existing combined custom-renderer launcher also receives this
fix for its supported fields. No new setting is required.

Ordinary field tool swings, bow shots and authenticated object-hit/break/push
actions may retain the 384x160 presentation even while the game temporarily
locks movement. Actual foreground-script cutscenes still use original framing,
including gaps between dialogue boxes. Unknown control owners remain native.
All existing source, animation, display/effect and map-boundary guards remain.

This is not a timer, an input-button guess, or a dialogue-pixel detector. Tool
and object lifecycle records determine presentation permission. It changes no
guest input flag, camera, collision, action timing, weapon selection or script.
The native center is still the unmodified original scanout. Game-specific code
is in this repository; engine/UI submodules and generated guest code are unchanged.

## Cause and traced facts

The previous `(flags & 0x1005) == 1` gate required free movement. A normal tool
action deliberately sets movement lock `0x1000` and clears free-input bit `1`.
It does **not** need foreground-script bit `4`. Consequently, each strike could
be mistaken for a cutscene: two A presses in the preserved reproduction each
caused exactly 20 narrow frames.

Guest data is interpreted only after supported-ROM identity, current field task
ownership and map-source authentication. See `FIELD_PROVENANCE_20260922.md`.

| Fact | Verified code/data |
| --- | --- |
| Field A dispatch | `0809D934` |
| Tool selection, seven values `0..6` | field root + `0x1EC7` |
| Action pool | root + `0x1FBC`, eight records, stride `0x1C` |
| Action fields | state `+0`, target `+2`, set flags `+0x12`, clear flags `+0x14`, allocation flags `+0x16`, Thumb callback `+0x18` |
| Control-mask accounting | `080A4FBC`; tool lock set/clear masks `0x1000/1` |
| Allocation/update | `080A5518`, `080A50F4` |
| Ordinary swing | `080A4C3C` creates callback `0809DA99`, states `0/1` |
| Bow continuation | tool `6`; `080A4724` consumes projectile request, `080A4D08` creates `0809E3ED`, states `0/1` |
| Object hit/break/push | `080A06E0` creates callback `080A0AD1`; target index `0..31`, object kind `2..9` in root + `0x1538`, stride `0x3C` |
| Object substates | `080A0AD0`, `080A1078`, `080A157C`; supported overall range `0..9` |

The new read-only `field_tool_action` predicate requires the lock-only control
mask `(flags & 0x1005) == 0x1000` and the current RAM flags to match the caller's
owner record. Every active action owning any relevant control bit must have the
traced allocation flags `1`, exact masks and an allowed callback. Passive
noncontrolling actions are ignored; stale/inactive slots grant nothing.
Concurrent swings and multiple object-hit actions are supported. Invalid
pointers, unknown tools/callbacks, unrelated lock owners and malformed records
decline. Object kind/substate and bow tool/substate checks further bound access.

The same predicate is used at capture and in live `objects_allowed()`. NPC
body/resource/shadow permission therefore no longer disappears merely because
a field tool locked movement. No extra simulation step or native replay was added.

When a tool/object interaction launches an attached event, guest code explicitly
sets foreground bit `4` and starts `08012E14`. That ends this exception immediately.
Deferred arbitrary-event callback `0809B849` is deliberately **not** allowlisted.

JP and English-beta ROM code bytes were compared and match in half-open ranges:
`0809D934..0809DB30`, `0809E3EC..0809E4A0`, `080A06E0..080A1078`,
`080A1078..080A14E0`, `080A157C..080A1860`, `080A4C3C..080A4D48`,
`080A4FBC..080A5170`. These are verified for the existing two ROM allowlist entries,
not a promise for other translation revisions.

## Evidence and rollback

Private evidence: `validation/field-tools-20260922/`.
Source captures: `playtest-20260922-130447-778`, frames `2129`, `2339`, `2471`.
All three caught active callback `0809DA99`, state `1`, flags `0x1000`.

- `native-regression2/report.json`: 748 paired frames, comprising the three
  exact captured actions, repeated swings and a movement/three-strike route.
- `expanded-objects/report.json`: the same 748 paired frames with normal widened
  NPC drawing enabled. Every recorded RAM/VRAM/palette/OAM hash and cycle count
  matched the pre-change executable in both test sets.
- All complete tool-test frames stayed wide. Per test set, 229 formerly narrow
  frames are now wide: `27 + 31 + 35 + 40 + 96`.
- The object route uses only ordinary input, without teleporting or editing
  memory. It hits two type-5 objects concurrently (indices `7/8`); both become
  inactive with exactly the same action, position and object metadata as before.
- `field-dialogue-regression/report.json`: 374 further paired frames across
  five known fields, two foreground-script samples, menu input and battle state.
  Recorded guest hashes/cycles and existing framing match. Both script samples
  remain native through all 41 frames each; walking into the village event still
  changes to native framing at the same point as before.
- 476 synthetic checks cover all seven tool selections, bow lifecycle, object
  types/states, multiple lock owners, invalid data and foreground-script
  precedence. Tests stay active in release builds and verify read-only behavior.
- All 35 configured CTests passed. No screenshot, pixel comparison, visual scene
  assertion, 16:9/2:1 test, or duplicate correctness-render frame was used.
  The owner-folder general-field launcher also passes its check-only run.

An initial validation assertion assumed every restored save began mid-raster.
These new captures resume on a complete-frame boundary. The failed first run is
retained at `native-regression/`; the validator now accepts either zero or one
initial incomplete frame while requiring all subsequent records and paired
completion identities. This was a test assumption, not a renderer fallback.

Executable: `build-native/Swordcraft3CustomRendererBeta.exe`, SHA256
`5e69429f363b9cc98951917de6d5e23f6e3ca8f9eeb63813c5f298c8aff17ec1`.
Exact previous executable: `validation/field-tools-20260922/before.exe`, SHA256
`1cac6dda5110d02d6fad201194315d84c59f9a6a04c984052487ac1ba6e99550`.
Do not run the archived executable directly without its normal runtime assets;
restore it to the configured build location to roll back. No saves were altered,
and no commit or push was performed.

## Limits

All seven selection values share the traced action machinery, but actual runtime
captures here exercise tool `1`, not a full playthrough with every weapon. Bow and
other tools have source-backed and synthetic coverage; further playtesting remains.

Some object actions include reward-message substates (`0806D314`/`0806D73C`)
without starting a foreground script. Those remain part of the tool interaction
and may stay wide, subject to existing rendering guards. Therefore the guarantee
is **foreground-script cutscenes remain unchanged**, not every text box narrows.
Special scripted tool interactions can still narrow; they must not be blindly
classified as gameplay simply because an A press or a tool animation preceded them.
