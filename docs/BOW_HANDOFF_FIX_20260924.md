# Bow handoff framing fix — 2026-09-24

## Result and scope

The pending-arrow handoff now remains wide in the full-viewport experiment.
Run `Launch Full Frame Combat 12x5 Test.bat` in this worktree. Root wrappers
still target the older custom-renderer worktree and were not silently promoted.

Only the game-owned field presentation predicate changed. No engine/UI,
generated guest code, camera, collision, bow timing, input-control flags or
save formats changed. The field still uses native-center protection; complete
combat composition is unchanged. No pixel/visual assertion or extra native
correctness redraw was added.

## Rule

After checking all active action owners, `field_tool_action` now also recognizes
tool 6 with `(field[1ED0] & 0700) == 0100`. This is the request phase consumed by
`080A4724`, not a timer or a cached preceding frame. Field bit `0040`, which skips
the consumer, prevents this new permission. Existing matching RAM/owner flags,
lock-only `1000` state, source/ROM authentication and foreground-script exclusion
remain required. An unknown or malformed active lock owner still rejects before
the pending request is considered.

The same predicate serves field framing and existing NPC presentation hooks,
so their permission does not drop during the handoff either. Existing draw and
resource hooks are not replaced. Tested paired runs retain exact recorded guest
state with those hooks enabled; this does not certify every crowded NPC scenario.

Source and captured flag evidence: [bow diagnosis](BOW_HANDOFF_DIAGNOSIS_20260923.md).

## Verification

- New unit cases first failed against the original predicate (6 failures),
  then passed after the correction: **971 field-action checks, zero failures**.
  Coverage includes all seven tools, all eight request phases, unrelated bits,
  request clearing, foreground scripts, suspended consumption, stale flags,
  unknown/conflicting action owners, malformed records and truncated memory.
- All **14 selected state-only CTests** passed. Legacy presentation/PNG audit
  tests were excluded, as in the accepted full-frame checkpoint.
- `tools/validate_bow_handoff.py` passed **1,110 paired frames** with normal
  expanded object submission at 384x160, full-combat and general-fields enabled.
- Three identical 180-frame runs of the first capture each exercise the restored
  shot plus two fresh input-triggered shots. Every completed field frame stays
  wide; each run recovers exactly three previously rejected handoff frames.
- The other two captures each run for 50 frames and recover one handoff frame.
  Their initial incomplete restore frame still waits for raster capture; all
  49 subsequent completed frames are wide. Eleven recovered handoff instances
  across repeated/overlapping tests are not eleven independent gameplay scenes.
- Five existing field layouts, two foreground-script captures, menu entry/return,
  an ordinary-tool route and combat R cycling retain their previous route and
  permission records. A field movement route still enters its legitimate event
  at the same point. Both script cases remain narrow; combat retains full ownership.
- Every recorded guest-region hash, cycle count, input/frame identity and
  field/action metadata agrees before/after. Original capture hashes are unchanged.
  Tests use separate loopback processes and scratch battery saves, not live play.
- The full-frame launch script's check-only path resolves the current inputs and
  executable without starting the game.

These are source/state/control-flow results, not independent rendering accuracy,
owner visual approval of this new binary, new FPS/audio measurements, all-bow-hit
interaction coverage or a full field-to-battle playthrough. No 16:9/2:1 runs.

## Build, evidence and rollback

Game base: `a3a082f32b90f4ec30b7f6b22ae5cb5405092661`, with this local change.
Engine/UI remain `f691f802` / `92fc0aea` and clean. The pre-existing audit/diagnosis
documents and normalized-unchanged `native-test.toml` status are preserved.

Rebuilt executable: `build-native/Swordcraft3CustomRendererBeta.exe`.
SHA-256: `a2f0ab964ffb995957ff650a25f1bd090220055dedb67afa3bb2da747edabbb5`.

Exact pre-fix executable: `validation/bow-handoff-fix-20260923/before.exe`.
SHA-256: `c29226f597d3a5fb0e4480f0db4cc43dc3ec348c676b6c9bcf5e4bbc0024215b`.
This private directory name reflects when work began before midnight.
Do not run the archived copy outside the configured runtime environment.

Private paired identities, logs and report:
`validation/bow-handoff-fix-20260923/paired/report.json`.
The executable was incrementally rebuilt using the configured private guest
object corpus; no guest source regeneration or dependency integration occurred.
The existing accepted hybrid build is untouched as an additional rollback.

The implementation turn did not commit, push or release anything. The owner
subsequently authorized publishing this fix and its audit/diagnosis records to
the existing full-viewport feature branch. This is not a merge into `main` or a
binary release. User saves are untouched.
