# Combat performance diagnosis — 2026-09-13

The owner reports approximately 50 displayed FPS with the accepted combat pilot.
The latest capture succeeded:
`validation/playtest-20260913-211013-984/frame-0000010239-1789348377379`.
Its report records native width 240 and host width 384. The saved screenshot was
visually inspected; it shows the recognized forest battle with expanded margins
and tan HUD wings. The report does not itself record FPS.

## Controlled measurement

`tools/benchmark_combat_tcp.py` runs the current executable through its local TCP
server with isolated saves and dummy SDL video/audio. Startup, snapshot loading,
initial warm-up, screenshot transfer and memory hashing are outside the timed
frame batches. Each mode runs three 240-frame batches, restoring the same source
state and stepping 30 warm-up frames before each. One initial 60-frame warm-up
also occurs in each process. Inputs remain released.

| Mode | Median processing time/frame |
| --- | ---: |
| Native, custom renderer disabled | 10.34 ms |
| Custom capture + native correctness replay, combat widening disabled | 16.25 ms |
| Custom combat, width 384 | 21.87 ms |
| Custom combat, width 284 | 20.97 ms |

These are headless processing times, not measurements of displayed FPS, vsync or
audio pacing. They nevertheless exceed the roughly 16.7 ms real-time budget in
both combat-wide modes, consistent with the owner's report. Sequential modes
and ordinary machine load introduce variability; do not present their differences
as exact per-function profiling or promise a particular speedup from one edit.

All twelve timed batches finish at the same guest-state hash. Combat logs report
no native mismatch or completed-frame fallback. Source state and executable
remain unchanged. Raw results, images and logs are in ignored
`validation/combat-performance-20260913/report.json` and sibling directories.

Executable SHA-256:
`7608a77b12c8c3c8bd9a213060b44c54b42df17da0bad64e0d4a89b23cbb2b52`.

## Source-level attribution / proposed next pass

The combat path currently renders the normal native frame, then a second native
frame in `GbaRasterCapture::draw_native`, then the full wide frame in `draw_view`.
Each visible scanline also copies IO, OAM, palette and all 96 KiB of VRAM into an
immutable capture (about 16 MiB per complete frame). The generic phase profiler
times live scanline rendering plus capture, but not the later replay/presentation
callbacks; its totals cannot be treated as the entire renderer cost.

The primary next target is redundant composition and capture overhead, not
altering parallax, reducing the simulation rate or skipping game frames. A safe
first experiment can use the already-rendered stock native image as the wide
center oracle, retaining strict replay as a diagnostic option. That alone may
not recover the entire frame budget. Follow with measured capture-buffer/replay
reuse or a margins-only composition path, retaining the native center and all
scene/HUD safety checks. Validate critical hits, pause, effects, both widths and
the accepted field/NPC-shadow goldens before changing the playtest default.

This turn adds only diagnostic tooling and this report. No game behavior,
renderer implementation, executable, launcher default, save, release or remote
branch was changed.
