# Complete-frame overworld test — 2026-09-24

## Status and launch

Built and state-tested, awaiting owner visual/audio playtesting. Not committed,
pushed, merged, or packaged as a release. The scenery-filter concepts are shelved;
this change contains no blur, decoration, new terrain, camera bias or collision edit.

In the owning Documents project folder, double-click
`Launch Full Frame Overworld and Combat 12x5 Test.bat`. Its small wrapper selects
the same-named launcher in `experiments/full-viewport-renderer`, opens settings,
then enables complete-frame fields AND supported complete-frame combat at 384x160.
F10 capture and recording remain available. The console prints the actual executable
path and SHA-256. Existing full-combat test saves/slots remain in
`build-native/full-combat-playtest`; no existing save was overwritten by this work.

`Launch Accepted Full Combat Rollback.bat` in the same owning folder selects the
preserved, previously accepted executable, with the bow fix, complete-frame combat,
and hybrid fields. It uses the same existing full-combat test save directory.
The two launchers must not be run concurrently against that save directory.

The older `Launch Full Frame Combat 12x5 Test.bat` explicitly disables full fields.
Other historical launchers/root wrappers were not redirected or removed.

## Identity

- Game worktree: `experiments/full-viewport-renderer`.
- Branch/base: `experiment/full-viewport-renderer-20260923`, `200b728` plus this patch.
- Final executable: `build-native/Swordcraft3CustomRendererBeta.exe`.
- Final SHA-256: `468e6915f2496f2f099284a5d09e55d0eb191552ca3fe6bc38e95d47b10fd1d6`.
- Accepted rollback SHA-256: `a2f0ab964ffb995957ff650a25f1bd090220055dedb67afa3bb2da747edabbb5`.
- Rollback executable and DLLs: ignored `validation/full-field-20260924/accepted/`.
- gbarecomp remains unchanged at `f691f802` on its dedicated primitive branch;
  recomp-ui remains unchanged at `92fc0aea`.
- No generated guest code, hooks, resource-allocation overrides or save format changed.
- `native-test.toml` retains its pre-existing normalized-identical dirty status.

This remains the private fast-iteration build using the existing checked guest
object corpus, not a distributable release. ROM/BIOS/captures/builds remain private.

## What changed

`FieldFrameRenderer` composes the entire supported field output from immutable
captured scanline state, using the existing reusable gbarecomp tile, OBJ, priority
and blend primitives. It does not use a replay PPU or paste native scanout over
its center. Successful field frames now claim `HostFrameOwnership::complete_frame`,
as supported combat frames already do. Field and combat retain game-specific scene
adapters/composers rather than pretending their source and UI policies are identical.

The existing field owner, ROM-source authentication, coarse/fine alignment and
per-placement animation reconciliation run before drawing. BG1..3 use those sources
across the whole width. Their origins are resolved once per layer/scanline, not once
per output column. BG0 is composed from captured hardware state only within the
native UI interval. All submitted native OBJ types handled by the shared primitives
are composed in the center; the accepted regular-body/shadow restriction remains
in force outside it. `CUSTOM_OBJECTS=0` disables extended objects, not native ones.

Field void masking is based on finite source bounds and combined authored BG1..3
opacity, not an RGB-black test. No authored coverage means black margins above
objects. An opaque source texel whose palette happens to be black is not classified
as void. This deliberately removes the legacy margin compositor's `background_color`
visibility heuristic; intentional opaque-black art would need explicit source
boundary metadata, not a new color heuristic. The paired gameplay corpus does not
prove every such authored-art case; inspect scene boundaries during owner testing.

Scene eligibility remains task/script controlled. Ordinary tools, R selection and
the bow handoff keep their existing permissions. Dialogue/menu/script fallback stays
native. Captures and ownership invalidate on reset/load; incomplete or unsupported
frames cannot claim full ownership. Mosaic OBJ frames additionally decline rather
than render unimplemented mosaic. No extra correctness replay, cosimulator or guest
CPU step was enabled. Stock native PPU execution/capture still runs; this migration
does NOT remove all underlying native-rendering cost.

`SWORDCRAFT3_FULL_FIELD_RENDERER=1` selects the new path. Absent/zero keeps hybrid
fields. The accepted executable is retained separately as a stronger rollback.

## Evidence

Final `tools/validate_full_field.py` run:
`validation/full-field-20260924/final-paired/report.json`.

- 1,124 paired frames, 14 cases, sampled guest hashes/cycles and state metadata
  identical to the accepted executable; same input snapshots and sequences.
- Five authenticated captured field layouts, horizontal movement/reversal,
  source-backed animation state, native and extended OBJ submission.
- Three existing tool snapshots, repeated bow strikes, R selection, two dialogue
  cases, menu entry/exit and two mid-sequence input-state reloads.
- Combat R/jump negative control retained exactly the previous combat route.
- 836 successful complete field frames: every one records 160 rows, 38,400 center
  columns and 23,040 extended columns. Every previously accepted wide field frame
  in these sequences obtained full ownership. No dialogue/battle frame was claimed
  by the field composer. Sources and input states were not modified.
- Some field sequences enter an existing native fallback; before/after field
  eligibility records match exactly. This is not an all-frame/all-room guarantee.

Final selected CTest set: 13 state/source tests passed. New field suite: 484,665
source/ownership/guard assertions, without reading output RGB. Includes center and
both-margin source calls, regular OBJ sampling, disabled extension, finite void
occlusion, opaque black-palette coverage, snapshot isolation, native width, and
whole-frame rejection of windows/fades/mosaic/incomplete captures. Saved log:
`validation/full-field-20260924/final-state-tests.log`.

An earlier broad test selection also ran two legacy synthetic capture/presentation
tests which inspect synthetic buffers. They are not part of the final state-only
evidence above and are not a gameplay image oracle. No gameplay screenshots or
framebuffer comparisons were requested or used as assertions in this migration.

## Performance

Initial baseline on field asset 350: about 2.9 ms/frame headless. Later runs showed
substantially different host timing; do not compare that initial number directly
against a later isolated result or call it displayed FPS.

`tools/benchmark_full_field.py` restores the same input before timed batches,
disables state traces/replay/phase profiling, uses dummy video/audio and isolated
saves, and runs before/after/after/before for each field. Final-build exploratory
report: `validation/full-field-20260924/final-benchmark/report.json`.

| Field source asset | Accepted pair medians (ms/frame) | Full-field pair medians |
| --- | --- | --- |
| 49 | 6.828, 6.544 | 8.637, 8.760 |
| 22 | 6.887, 6.669 | 8.908, 8.779 |
| 29 | 6.441, 6.416 | 8.523, 8.714 |
| 350 | 6.352, 6.334 | 8.712, 8.647 |
| 364 | 8.226, 8.352 | 9.307, 9.436 |

This indicates additional composition work, approximately 1–2.4 ms/frame in these
runs, not a speedup. A brief separate unit-test invocation overlapped part of the
exploratory benchmark session, so these are not controlled displayed-FPS numbers.
No FPS/audio pacing or long-session stability claim is made. The normal game still
needs owner testing on screen with audio; performance tuning remains a follow-up.

A subsequent isolated rerun, with no concurrent project builds/tests, confirmed the
direction of the overhead (`validation/full-field-20260924/isolated-benchmark/report.json`):

- Field 49: accepted 6.883 / 6.506 ms; full field 8.993 / 8.557 ms.
- Field 364: accepted 8.366 / 7.900 ms; full field 9.182 / 9.599 ms.

Every paired benchmark endpoint retained the same recorded guest state. These are
still headless processing measurements, not windowed FPS or audio validation.

## Owner test / remaining limits

Follow-up: the playable binary was subsequently rebuilt with a selective engine
diagnostic-overhead backport. See [UPSTREAM_PERFORMANCE_20260924.md](UPSTREAM_PERFORMANCE_20260924.md)
for its new executable identity, actual windowed FPS comparisons and the
pre-performance rollback launcher. The historical measurements above are retained
as measurements of the earlier binary, not the updated build.

Walk through the lake and village, watch water and NPC bodies/shadows across both
native-screen edges, use tools/bow and R selection, open/close dialogue and menus,
enter a battle, then load a state. Use F10 if anything differs or narrows unexpectedly.
Watch real FPS and audio rather than interpreting headless throughput as a guarantee.

This does not authorize new map formats or spell families. Unknown field scripted
animations, unsupported raster modes/effects and unauthenticated sources keep their
existing fallback. Combat arena/effect coverage, historical chest/rewind crashes,
upstream merge work, finite scenery aesthetics and release packaging are unchanged.
