# Windowed FPS benchmark — 2026-09-24

## Result

The new complete-frame field build remains close to the accepted build's actual
windowed FPS. The tested combat scene remains somewhat below the GBA's 59.7275 Hz
target in BOTH builds. This is not an optimization or a claim of locked 60 FPS.

| Test | Accepted FPS, two runs | New FPS, two runs | Mean accepted / new |
| --- | --- | --- | --- |
| Opening lake (field source 49) | 59.68, 59.67 | 59.60, 59.58 | 59.68 / 59.59 |
| Larger field (source 364, 632x616) | 59.66, 59.71 | 59.62, 59.55 | 59.69 / 59.59 |
| Rocky combat, normal code-cache behavior | 59.10, 57.81 | 58.82, 58.27 | 58.46 / 58.55 |

Combat results vary more between repetitions than between builds. The declining
run-order trend occurs in both builds; the cause was not isolated. Do not claim a
combat improvement or regression from the approximately 0.09 FPS mean difference.

Frame spacing is not perfectly even. Lake p95 intervals increased from
22.31–22.58 ms to 24.39–24.40 ms. Larger-field p95 increased from 22.20–22.54 ms
to 23.00–23.39 ms. Normal combat p95 ranges overlap: accepted 18.40–19.46 ms,
new 18.83–19.33 ms. A near-60 mean therefore does not imply stutter-free motion.

## Method and coverage

- Real SDL window and audio output; Direct3D 11, nearest scaling, VSync off as in
  current runtime defaults. Reported desktop display: 1707x1067 at 165 Hz.
- 384x160 host output (12:5), scale 3. Same staged settings and inputs for both
  binaries. Source state hashes and executable hashes are recorded in JSON.
- Each session runs approximately 25 seconds after a confirmed state load.
  Discard the first three seconds of present samples. Order: accepted, new,
  new, accepted. These are short-session tests, not long thermal soak tests.
- FPS = 1,000 divided by mean present-to-present interval in milliseconds.
  This uses existing SDL-present timestamps, corresponding to the title-counter
  measurement. It is not headless processing capacity or proof that every frame
  was physically scanned out by the monitor. DWM counters alone do not prove VRR.
- Neutral guest input is replayed. Ordinary Assist/rewind overhead is retained;
  exclusive replay is NOT enabled. No fast-forward, screenshot export, per-frame
  state tracing, extra correctness replay, cosimulator, or simultaneous build.
- State/route counters, not image comparisons, establish active coverage: new
  fields report complete-frame composition, field counters advance without
  fallback, and both combat binaries report complete battle frames with no native
  reuse. The single initial incomplete capture in some runs is before warm-up.
- No game was already running. The harness closed only its own child windows.
  ROM copies, battery files, settings copies and cache copies are isolated under
  validation. Existing states, caches, ROM, and user saves were not modified.
- This is saved-state playback, NOT a replay or FPS extraction of the supplied
  `20260924-1723-15.9824770.mp4` recording. No player attack/movement sequence was
  injected. The historical fixture name `battle-r-jump` identifies its source;
  this benchmark did not perform R cycling or jumping. It does not certify
  worst-case Flare Burn, other spells, map traversal or fullscreen performance.

## Audio and code-cache control

All recorded audio probes reported zero buffer underruns and zero overflow drops.
That does NOT mean audio was perfect: normal combat required 40–464 ms cumulative
time-stretch by the 24.1-second probe across the four runs (new: 122 and 315 ms).
These are bridge diagnostics, not an independent listening test.

The initial controlled comparison disabled automatic self-heal compilation in both
builds. Its combat results were accepted 58.71/57.99 and new 58.83/58.06 FPS. The
overworld cases stayed fully static without interpreter misses. Combat was then
rerun with normal self-heal enabled and identical copies of the existing playtest
cache; the table above uses that second combat comparison. The normal runs reported
zero warm-loaded entries, three dispatch misses and about 18 million interpreted
instructions per session, with no healed native calls. These RAM bridge paths are
a profiling lead, NOT an established cause of the pacing deficit. The benchmark
did not enable experimental RAM-overlay healing or change runtime code.

## Reproduction and artifacts

Harness: `tools/benchmark_windowed_fps.py`. Example from this worktree, using a
fresh output directory:

```text
python -B tools/benchmark_windowed_fps.py --output validation/windowed-fps-new-run --seconds 25 --passes 2 --cases battle-r-jump --normal-cache
```

Keep the game controls untouched while it runs. It stages binaries, opens audible
game windows, and closes each own window automatically. `--normal-cache` copies
the existing playtest cache and enables normal self-heal; omit it only for the
explicit controlled self-heal-disabled comparison. All test outputs are private.

- `validation/windowed-fps-20260924/report.json`: twelve controlled sessions;
  each child folder contains present cadence CSV and stdout/stderr.
- `validation/windowed-fps-20260924-combat-normal/report.json`: four normal-cache
  combat sessions, with the same per-run artifacts.
- `validation/windowed-fps-20260924-pilot/`: two short harness checks, excluded
  from the final result table.
- Accepted binary SHA-256:
  `a2f0ab964ffb995957ff650a25f1bd090220055dedb67afa3bb2da747edabbb5`.
- New binary SHA-256:
  `468e6915f2496f2f099284a5d09e55d0eb191552ca3fe6bc38e95d47b10fd1d6`.

No renderer/runtime/gameplay changes, rebuild, commit or push in this benchmark.
The port workflow's state-based coverage and live-presentation measurement rules
were used; historical pixel-comparison guidance was not applied.

## Follow-up: where combat time goes

`validation/windowed-fps-20260924-combat-phases/` adds one 25-second normal-cache
session per binary. The harness now exports the existing always-recorded
`GBARECOMP_FRAME_PHASE` ring; no runtime rebuild or additional per-frame profiler
was introduced. Actual present FPS: accepted 58.47, new 58.40.

Phase analysis below discards the first 180 records (approximately three seconds):

| Phase, wall-clock milliseconds | Accepted mean | New mean | New p95 |
| --- | --- | --- | --- |
| Guest advancement INCLUDING native scanout, raster capture and custom composition | 14.97 | 15.03 | 16.24 |
| Final host surface composition/copies | 0.03 | 0.03 | 0.04 |
| Window presentation, including upload/draw/present | 1.28 | 1.28 | 3.33 |
| Audio push | 0.09 | 0.07 | 0.02 |
| Input pump/rewind work | 0.07 | 0.06 | 0.18 |
| Frame-pacer wait | 0.66 | 0.66 | 1.77 |
| Game-thread on-demand compilation (overlapping diagnostic) | 0 | 0 | 0 |

The misleadingly named `guest_us` column is NOT CPU emulation alone: the custom
renderer is invoked by `g_native_frame_presenter` during PPU advancement before
the host-present hook starts. Do not conclude that custom rendering takes only
0.03 ms or that the entire 15.03 ms is interpreter time. All columns are wall
time, so scheduling delays can appear within them.

In the new run, non-pacer work averaged 16.47 ms against a 16.743 ms budget;
381 of 1,296 post-warmup samples (29.4%) exceeded that budget. There is very little
headroom for variable presentation/processing delays. Current `FramePacer` code
resets its deadline when more than 1.5 ms late, deliberately avoiding a short
catch-up frame; such resets discard accumulated time and can lower average FPS.
Reset counts themselves were not instrumented in this run, so their exact share
of the deficit is unquantified. Audio stretching is consistent with the shortfall,
not evidence that audio processing is the main bottleneck.

This establishes the aggregate frame-budget problem, not the hottest function
within guest advancement/composition. Three RAM interpreter bridges still occur
(about 18.27 million interpreted instructions in the new run); finer profiling is
needed to separate their cost from stock scanout, capture, custom composition and
host scheduling. No optimization, frame skipping or gameplay-speed change applied.
