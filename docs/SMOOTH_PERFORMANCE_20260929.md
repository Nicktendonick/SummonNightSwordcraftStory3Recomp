# Smooth 2x combat performance — 2026-09-29

Local test update only: no commit, publication or public-release replacement.
Follow-up to `GUARD_DIAGNOSIS_20260929.md`.

## Confirmed bottleneck

Disabling Smooth 2x restores approximately normal battle FPS on this PC;
enabling the old implementation causes a large drop at 384x160. Guard remains
enabled in both cases. This identifies smoothing as the large measured cost,
not evidence that Guard caused the reported slowdown.

Smooth 2x is a CPU filter, not a GPU shader. At 384x160 it computes 245,760 output
samples every frame, including neighbor-distance decisions and weighted sums.
Widescreen has 60% more source samples than native 240x160. Battle processing
already uses most of a roughly 16.74 ms frame budget, leaving insufficient time
for the old scalar filter. Native width has more headroom in this fixture.

## Matched toggle and before/after results

Same authenticated English battle snapshot, neutral exclusive replay, 3x window,
Raw color, screen effects Off, real SDL Direct3D11 and audio, Guard enabled.
Each run presents 600 frames; first 90 timing intervals excluded. Two passes
reverse case order. No concurrent build or second game was intentionally run.
Separate state/ownership audits are excluded from timings.

| Width | Build / scaler | Pass 1 FPS | Pass 2 FPS |
| --- | --- | ---: | ---: |
| 240 | Previous / Nearest | 59.70 | 59.74 |
| 240 | Optimized / Nearest | 58.85 | 59.67 |
| 240 | Previous / Smooth 2x | 59.43 | 59.60 |
| 240 | Optimized / Smooth 2x | 59.70 | 59.67 |
| 384 | Previous / Nearest | 59.29 | 58.83 |
| 384 | Optimized / Nearest | 59.06 | 59.27 |
| 384 | Previous / Smooth 2x | 52.37 | 54.89 |
| 384 | Optimized / Smooth 2x | 58.92 | 59.38 |

Wide Smooth presentation preparation fell from 5.38–5.42 to 1.79–1.81 ms/frame
(about 67% lower). Whole presentation fell from 5.45–5.57 to 1.99–2.01 ms.
Guest processing stayed in the same 12.60–13.32 ms range. New opt-in counters
measured scaling itself at 1.537–1.541 ms and upload at 0.100–0.105 ms/frame.
Upload is not the dominant cost here. All runs had zero filter-resource fallbacks.

Earlier old-build toggle tests measured 46.14–46.79 FPS with about 14.7 ms guest
processing. This later session was faster even before the fix: do not mix sessions
into a claimed matched 46-to-59 improvement. CPU/load/scheduling vary. One native
Nearest run also had a single 126.6 ms gap. This is one developer-PC encounter,
not a full-game, physical-input latency, or guaranteed-60-FPS certification.
A separate 100-frame synthetic scaler measurement improved from 5.815 to
1.697 ms/frame; that supports the CPU diagnosis but is not game FPS.

## Implementation

- SSE2 processes four source samples together and reuses neighbor distances.
  SSE2 is baseline on the existing x64 target; other targets retain scalar code.
- Current-frame RGB24 is packed safely into reusable RGB32 scratch with duplicated
  borders. Every entry is rewritten each call: no stale-frame reuse or skipping.
- Existing thresholds, weights and exact rounding are retained. SIMD uses the
  same bounded reciprocals already present in the scalar implementation.
- Unaligned loads/stores and scalar tails support odd widths. Source bounds,
  size limits, alias rejection and allocation-failure handling remain intact.
- `GBARECOMP_SMOOTH_PROFILE=1` enables separate scale/upload timing. Normal play
  does not execute those additional profiler clock calls.
- No new dependency, guest-code change, Guard remapping change, battle timing,
  camera change or renderer-ownership change is part of this optimization.

## Verification

- 20 rebuilt state/math/resource tests passed, including SIMD, forced scalar,
  input, pause menu, language, preferences, Guard, battle/field/lake gates and
  provenance/route audits.
- Exhaustive unsigned-byte distance/threshold and bounded normalization tests
  cover lane isolation and integer rounding. Protected immutable source pages
  cover both boundaries, tiny/odd sizes, 240/284/384 widths and scalar tails.
- SDL integration exercised all 24 scaler/effect combinations at native and wide
  sizes, fractional Sharp and graphics-resource reset recovery.
- English/Japanese live-menu checks passed pause PC/cycle invariance, filter
  edits, cold Reset persistence and launcher edit/reopen persistence.
- Exact stripped candidates passed 20 Guard cases / 2,017 guest frames: native
  B/player-state parity, six R slots, hold/release, jump/pause gates, ordinary-input
  equality, AI cancellation, state restore/release, field Select noninterference
  and Japanese cold-boot/noncombat equality. Japanese combat remains unverified;
  English states were not loaded into the Japanese corpus.
- Performance audits reached the battle hook 89 times each; both old/new wide
  runs produced 88 complete expanded combat frames. Native width intentionally
  does not produce expanded frames.
- Assertions used math, memory, state and counters; no screenshots, output-color
  or framebuffer comparisons. Existing pixel-comparison tests were excluded.
  This is not human visual approval or a complete playthrough.

## Local installation and rollback

Only two files changed in the owner's existing
`release/Guard Experiment 20260929-023325-a2fa/Runtime/`:

- `Swordcraft3CustomRendererBeta.exe`, SHA-256
  `324057340aaa3667cb22118b65a63f720c1ba318d72c57a61417af0a12d1f4ee`
- `Swordcraft3Japanese.exe`, SHA-256
  `c72c0c7afd6c2e4a56760ae918a9f21ffce6df055c93a67362dc587ab27148bf`

Debug stripping preserved loaded PE sections, imports and entry points.
Dependency closure and read-only launcher preflight passed. All 154 other files
were hash-checked unchanged, including saves, settings, states, ROMs, BIOS,
credits, artwork and launchers. The original `TEST-PACKAGE.json` is historical;
new installation identity is in the update report. No user process was stopped.

Rollback copies and `UPDATE-REPORT.json` are in the owner project's
`release/Guard Performance Rollback 20260929-041514-3f7f/`.
Normal `release/Portable Beta` and public packages were not updated. Open the
same `Swordcraft Story 3 Guard Test.exe`; Smooth 2x can remain enabled.
Broader playtesting should precede promotion. Mine Select/width fallback and
unsupported arena 8 are separate, unfixed issues.

## Reproduction and private evidence

Run `tools/benchmark_smooth_optimization.py --old <previous-engine> --new
<candidate-engine> --frames 600 --passes 2`. After installation, use the rollback
engine as `--old`. Private source inputs were hash-checked unchanged. Do not
publish validation folders; they may contain owned states and local input paths.

- `validation/smooth-optimization-1790654343236401800`: 16 timings, 4 audits,
  phase/cadence CSVs and unchanged input/executable identities.
- `validation/filter-menus-20260929T040812723`: both-language menu/reset checks.
- `validation/guard-1790655157339863800`: exact-candidate Guard validation.
- `validation/smooth-engine-candidate-20260929-041159-f8b3`: stripped staging and
  complete pre-update target snapshot.
- `build-native/Testing/Temporary/LastTest.log`: selected CTest results.
