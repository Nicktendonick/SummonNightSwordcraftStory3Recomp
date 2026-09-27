# Selective upstream performance update — 2026-09-24

Subsequent launcher-only follow-up: [BETA_LAUNCHER_20260924.md](BETA_LAUNCHER_20260924.md)
records the icon-branded rebuild and native executable entry point. Measurements
below retain their original executable identities; no renderer/performance code
was changed by that branding and launcher-preview follow-up.

## Scope and provenance

The full-viewport game worktree retains the accepted complete-frame custom renderer
for field and combat. This pass changes reusable engine diagnostics, not scene
recognition, spell handling, camera limits, object submission, or the 12:5 viewport.
Pre-existing uncommitted field-renderer migration work is preserved.

Engine work is isolated on `perf/opt-in-diagnostics-20260924`, based on
`f691f802d9078f6a23b76f4ce25075f7584de288`. Selected compatible changes come from
upstream `1ea864712561eed358882f48f53701ce4b078f79` (opt-in expensive diagnostics).
See `gbarecomp/docs/UPSTREAM_DIAGNOSTICS_BACKPORT.md` for included features, explicit
diagnostic flags, and deferred compiler/overlay ABI changes. This is NOT a wholesale
update to upstream main. Existing generated calls return early when recording is
disabled; the newer emitter's inline guards have not been regenerated into them.

Upstream checked this date:

- GBARecomp main: `c83969593fd3cbd53a1098f0d2044afbc6062c52`.
- recomp-ui master: `1b3292f8b93c71843d9503540c6b3dea8404a904` (now under
  RetroPortingToolKit). Local UI remains `92fc0aea585a933e92db17d5079d6d09b6600613`.
  Its larger launcher/API update is deferred; no direct combat runtime improvement
  was established from that update. Do not describe both dependencies as fully
  up to date.

Default-off recorders: runtime trace, MMIO history, audio FIFO history, frame-phase
timing, present-cadence/DWM measurements, and the hang watchdog. Diagnostic capture
can explicitly enable them. F10 state/source captures remain available, but optional
trace histories are empty unless armed before launch. Raw color passthrough also
avoids constructing an unused lookup table. No extra renderer replay/cosimulator
was enabled during gameplay.

## Build identities and rollback

- Before: `468e6915f2496f2f099284a5d09e55d0eb191552ca3fe6bc38e95d47b10fd1d6`.
- After: `6f5caac0f75ec9f01da3b0bb4f1086ba15e9cf618518dbd4f76e3a08bd653833`.
- Updated binary: `build-native/Swordcraft3CustomRendererBeta.exe`.
- Preserved before binary and runtime dependencies:
  `validation/upstream-performance-20260924/before/`.

From the owner project folder, run **Launch Full Frame Overworld and Combat 12x5
Test.bat** for the updated build. **Launch Pre-Performance Rollback.bat** runs the
immediately preceding complete-frame build. Both use the existing isolated
full-combat playtest save folder; do not run them simultaneously. The older
accepted-combat/hybrid-field rollback remains available separately.

These are private test builds using the existing generated guest object corpus,
not redistributable release packages. ROM, BIOS, saves, caches and binary validation
artifacts are not to be committed. No push or release was requested for this pass.

## Correctness evidence

- Build and selected test targets succeeded (`validation/upstream-performance-20260924/build-tests.log`).
- All 22 selected unit tests passed (`unit-tests.log` in that directory): diagnostic
  disabled/enabled, codegen, audio/config/state, renderer source/policy and game
  field/combat ownership/effect/action tests.
- 14 paired scenarios, 1,124 paired frames, 836 complete field frames passed in
  `paired/report.json`. Both versions enable full-field/full-combat composition.
  Guest-state records, cycles, field eligibility and complete-frame ownership
  match, including movement, scripts, menus, restore, tools, bow and combat R/jump.
- Default-off runtime and MMIO diagnostics report `enabled:false` with zero
  history entries in the updated game TCP captures. Diagnostic-on/off unit tests
  verify MMIO/FIFO behavior and runtime trace gating.
- An additional 10-frame field run with runtime/MMIO/FIFO tracing enabled retained
  identical guest hashes, cycles, field ownership and action state to the same
  initial 10 frames with tracing disabled. Runtime and MMIO TCP queries reported
  `enabled:true`, with 4,096 and 233 entries respectively. Evidence is in
  `diagnostics-enabled/`. Endpoint provenance is collected on the final frame of
  each sequence, so that optional report field is not compared between sequences
  with different lengths (10 versus 74); all per-frame guest/state fields match.
- No gameplay pixels, framebuffer comparisons or screenshot assertions were used.
  These bounded state tests do not establish universal scene coverage or fix the
  historical chest/rewind issues.

## Windowed performance method

`tools/benchmark_windowed_fps.py --before-full-field --before-exe <preserved exe>`
compares identical complete-frame renderers, isolating this engine update. The
benchmark uses real SDL windows and audio, 384x160 at 3x, copied settings, private
saves/caches, neutral guest input, and before/after/after/before ordering. Each run
lasts 25 seconds after confirmed state load; the first 3 seconds are excluded.
The game is frame-limited normally, not run as headless TCP throughput.

Present cadence, frame phases and audio probes are explicitly enabled on BOTH
binaries for measurement, while the new runtime/MMIO/FIFO trace gates are off.
Thus default-off phase/cadence savings are not measured here. No build or other
game tests run concurrently. SDL present-entry cadence is not a hardware scanout
measurement. These short neutral-input scenes do not cover every active spell or
long-session behavior. Native GBA target is about 59.73 FPS.

Detailed runs: `validation/upstream-performance-20260924/windowed/report.json`.
The `guest_us` phase includes guest execution, native scanout/capture and the custom
renderer callback; it must not be labeled pure CPU emulation time.

## Windowed results

Two 25-second runs per binary per scene (12 runs total), in the order above:

| Scene | Before FPS, individual runs | Updated FPS, individual runs | Mean before → updated |
| --- | --- | --- | --- |
| Rocky combat | 57.29, 58.64 | 59.09, 58.73 | 57.97 → 58.91 |
| Lake field (49) | 59.71, 59.72 | 59.71, 59.71 | 59.72 → 59.71 |
| Larger field (364) | 59.70, 59.63 | 59.65, 59.65 | 59.67 → 59.65 |

Combat's mean of per-run 95th-percentile frame intervals improved from 19.93 to
18.67 ms. Its guest-plus-rendering phase averaged approximately 15.28 ms before
versus 14.09 ms after (first 180 phase records excluded per run); some time saved
became ordinary frame-pacer waiting. This supports reduced processing overhead,
but the unequal baseline runs show substantial host variability. The roughly
0.94 FPS mean gain is a short-session observation, not a guaranteed improvement
in every battle. Updated combat still falls below the native 59.73 FPS target.

Field average FPS is essentially unchanged. Frame pacing is not fixed: lake p95
intervals remain about 24 ms. The larger field's mean p95 was 21.59 ms before and
23.16 ms after; its last baseline run was already 23.08 ms, close to the updated
runs, so these data do not isolate a repeatable regression or a pacing improvement.
Further profiling should separate guest execution from custom composition and
investigate late-frame pacing; merging the launcher is not a demonstrated remedy.

All 12 final audio-probe samples reported zero bridge underruns and zero overflow
drops. Combat still used time stretching: the final samples recorded 768/178 ms
before and 32/198 ms after. Field samples recorded none. This is runtime counter
evidence, not a listening assessment or proof that all audio artifacts are fixed.

Changes remain local on the dedicated engine branch and the existing game
experiment branch; no GitHub push or release was made in this pass.
