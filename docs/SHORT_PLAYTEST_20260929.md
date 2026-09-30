# Short scripted playtest — 2026-09-29

Tested the already-installed private Guard engine, not a new build. No runtime,
gameplay, release package or GitHub change was made during this playtest.
Diagnostics use authenticated game state, input traces and resource/timing
counters, not screenshots, framebuffer comparisons or scene recognition.

## Result

The targeted gameplay/menu checks passed. The saved presentation combination
(Classic color, Smooth 2x, LCD Grid at 35%, 3x window, 384x160 host) averaged
58.31–59.32 FPS across five short combat routes. This is a useful smoke result,
not proof of a full-game 60-FPS lock or perceptually perfect audio.

| Saved-settings route | FPS | p95 interval, ms |
| --- | ---: | ---: |
| Rocky actions | 59.32 | 17.85 |
| Forest actions | 59.23 | 17.81 |
| Arena 2 actions | 59.03 | 18.20 |
| Spell-effect snapshot | 59.10 | 18.25 |
| Critical-effect snapshot | 58.31 | 18.81 |

All five had zero filter-resource fallback frames and no measured intervals over
33.5 ms after warm-up. Real audio ran throughout. The final audio probe in the
critical route reported 46 ms cumulative time-stretch over 15 events; other
routes' last probes reported zero. All reported zero bridge underruns and
overflow drops. Counters do not establish that the output sounded crackle-free.

A preceding clean Raw-color/effects-Off pass measured 57.01–59.40 FPS. Earlier
test-development runs measured 56.61–59.15. These sequential runs vary with CPU
load/scheduling; they do not show that LCD improves speed. Self-heal compilation
was disabled for repeatability. No compile time was reported by the probes.

## Active coverage

The clean state audit inspected 3,364 guest frames across eight routes:

- Arenas 3, 0 and 2: movement, A attacks, jumping, R cycling, B ability input,
  repeated Select Guard, native Start pause/resume. Each route inspected 537
  frames, with 536 complete widened combat frames, 84–87 guarded frames, no
  stuck Guard at release, both R slots 0/1 observed, and varying jump height.
  Native pause mode occurred for 53 frames per route; ordinary play resumed.
- Effect coverage included script effect kind 2 for 157 frames, critical kind 7
  in both an action route and a restored critical snapshot, and kind 9 in arena 2.
  Source-owned affine composition was observed. This is not every weapon/spell.
- Victory exit progressed through lifecycle 5, 7, 8 and 0. Combat ownership was
  released and the field script became the owner. Actor memory is repurposed
  outside combat; its bytes must not be decoded as Guard/AI/ability state then.
- User slot 3: arena 8 resumed with Start, guarded for 30 frames, and released.
  Its renderer remained on the documented unsupported-arena native fallback.
- Mine slot 5: 30 wide free-control frames, followed by 83 field-script fallback
  frames after Select. No combat ownership was claimed in the field.

Separate real-window runs replayed the five combat schedules with Smooth 2x and
real audio. Runs contained 537 or 600 presents, excluding 60 warm-up intervals.
The three action routes include native pause periods; FPS is route-wide, not a
physical-input-latency measurement. State audits are not included in FPS timing.

The Esc-menu route passed:

- Auto-pause freezes guest frame/cycles/PC; Resume advances them again.
- With auto-pause disabled, guest state advances while the menu remains open.
- Separate manual Pause and Resume freeze/restart the guest.
- Native 240, approximately 16:9 284, and 12:5 384 host widths apply after resume;
  guest width stays 240. Resizing is deferred to a safe frame boundary, not
  applied immediately during a frozen guest frame.
- Smooth 2x can be disabled/re-enabled while paused, without advancing the guest.
- A private slot 9 is saved and restored successfully. It is not the user's slot.
- 720 gameplay presents plus paused-menu redraws completed with no filter fallback.

## Test corrections and data safety

The first long input schedule defeated the enemy before R/pause checks, so it
was not counted as covering them. The final schedule checks those controls
before attacks. Absolute PPU frame counters anchor replay traces directly.

The first menu attempt incorrectly expected resizing while paused and omitted
the isolated Save States directory normally created by the portable starter.
Those were harness errors, not established player-facing regressions. The
corrected menu-only rerun and the subsequent complete run both passed.

The first headless audit also exposed a diagnostic isolation bug: generic ROM/
BIOS pointer caches use the executable directory, whereas coverage files use
cwd. The shared `Session` helper now explicitly supplies an isolated portable
data root and the portable adapter flag, disabling generic sidecar persistence.
The two newly created Runtime pointer files were moved, not deleted, to:
`validation/short-playtest-1790657820683159600/recovered-diagnostic-sidecars/`.
The actual launcher pointers in Settings, ROM/BIOS contents and original save
inputs remained unchanged. Subsequent complete runs hash-checked every file in
the installed Guard folder unchanged, as well as all source inputs.

The engine SHA-256 remained
`324057340aaa3667cb22118b65a63f720c1ba318d72c57a61417af0a12d1f4ee`.
No user's running process was stopped. All test-owned processes exited.

## Limits and next work

- Mine Select/field-script width fallback and unsupported arena 8 remain open;
  this test neither bypassed their guards nor implemented either renderer fix.
- Some slower runs required audio time-stretching. Perceptual audio quality and
  physical controller feel were not evaluated by the automated test.
- English combat only; no Japanese combat approval, full-game playthrough,
  independent original-hardware oracle, or fresh field-to-battle encounter claim.
- Safe to continue beta evaluation of this performance fix; do not treat this
  record alone as public-release approval. Main Portable Beta is unchanged.

## Evidence and reproduction

Private evidence must not be uploaded; it includes local input paths and states.

- `validation/short-playtest-1790659110032262300/report.json`: complete clean
  eight-route audit, five Raw/Off window runs, and successful menu/state checks.
- `validation/short-playtest-1790659381411952000/report.json`: five saved-display
  combination runs, source-identity match to the audit and unchanged player files.
- `validation/short-playtest-1790658865232754600/report.json`: corrected menu rerun.
- The earlier `1790657820683159600` and `1790658567542495100` directories retain
  incomplete harness-development attempts, not an all-checks-passed result.

Run `python -B tools/playtest_smooth_guard.py` for the full baseline. Then run
`python -B tools/playtest_smooth_guard.py --windows-from <baseline-report.json>
--screen classic --effect 1 --strength 35` for the user's display combination.
Run alone, with no other game instance or build competing for the CPU.
