# Optional presentation filters — 2026-09-28

## Player-facing behavior

Launcher Settings and in-game Esc > Graphics expose four scaling modes:
Nearest (original), Linear, Sharp fractional and Smooth 2x. A separate screen
effect selector offers Off, LCD Grid and lightweight CRT, with 0–100% intensity.
LCD and CRT are alternatives, not simultaneous overlays. Existing screen-colour
presets remain available and can be combined with the new options.

The default remains Nearest, Raw colours and effects Off. Smooth 2x is an
independently implemented, conservative edge-aware quarter-pixel interpolator,
not xBRZ. No xBRZ implementation or shader was incorporated. This avoids adding
that dependency; it does not resolve the separate existing release-review items.
CRT means scanlines, grille and a subtle vignette, not full physical CRT emulation.

Only the final game image is filtered. Guest execution, input timing policies,
authored scene bounds, camera and save formats are not changed. The native
launcher and Esc-menu text are unfiltered. The game's own text is part of the
game image and can look softer with smoothing.

## Portable preferences and fallback

The five presentation keys live in the portable Settings/launcher.ini:
linear_filter, sharp_filter, smooth_filter, screen_effect and
screen_effect_strength. Runtime changes atomically update only these keys,
preserving other sections, comments, encoding and unrelated settings. Failed
writes leave the current choice unchanged. Launcher close and Play persist the
same choices. Reset Game rereads the keys before starting the next process, so
old launch arguments cannot restore a stale filter choice.

Launcher loading/saving also recognizes a first-line UTF-8 BOM. Regression tests
exercise Esc-writer -> launcher-reader/writer -> Esc-writer roundtrips without
duplicating the Launcher section, dropping the BOM or losing unrelated keys.

Scaling and effect resources are cached. Size/settings changes and SDL device
reset events invalidate them. Unsupported allocation/draw paths fall back to
the original texture rather than blocking play; failed smoothing is not retried
each frame. The underlying scaler never mutates its input. Invalid dimensions,
aliasing and oversized allocations are rejected. Effects are composed before
the native overlay UI.

## Verification completed before packaging

- Both English-beta and Japanese engines rebuilt incrementally.
- Targeted CTest coverage passed for scaler/mask allocation, immutable guarded
  input, 1-pixel dimensions, rejection paths and buffer reuse; atomic preference
  updates; launcher capability/default/selection/persistence/argument contracts;
  reset preference refresh; and existing runtime input/presentation behavior.
- SDL integration exercised all 24 scaler/effect combinations at native 240x160
  and expanded 384x160, zero-strength effects, open native menus, fractional
  Sharp scaling at an 800x600 window and graphics reset resource recovery.
  Engagement counters verified the paths, with no fallback in supported cases.
- tools/test_presentation_filters.ps1 passed for both language engines:
  Smooth/CRT live changes, saved strength, actual menu pause state, cold Reset,
  launcher reload, Sharp/LCD changes and subsequent reload. Unrelated input
  preferences and the original private ROM/BIOS files were unchanged.
  Evidence: validation/filter-menus-20260928T220653282 and final-build rerun
  validation/filter-menus-20260928T222049032.
- Scaler and preference helper tests also compiled under C++17; project targets
  use C++20. Constant arithmetic normalization is checked at compile time over
  all 17,889 supported scalar cases.

Assertions use memory/control-flow, bounds, resource state and counters. No
screenshots, pixel comparisons or framebuffer comparisons were used. This is
not a claim of human visual approval of every filter or a full-game playthrough.

The broader 31-case CTest selection passed, including combat/field/lake
state tests, portable input/language tests, save configuration, audio DRC,
mod-state/audio, diagnostic capture, replay primitives/policy and code generation.

## Combat timing

Reproduce with tools/benchmark_presentation_filters.py --audit-state --frames
360 --warmup-frames 60, using a fresh output directory. Private inputs are
resolved under the owner project and hash-checked unchanged. Each timing run
uses real SDL Direct3D11 presentation/audio, neutral input, the same verified
combat save state and a 3x window. Separate state audits authenticate the battle
hook and the expanded frame's owned rows/regions; they are excluded from timing.

Evidence: validation/presentation-filters-20260928/report.json and report.md.

| Source width | Scaling / effect | FPS | p95 gap (ms) | p99 gap (ms) |
| --- | --- | ---: | ---: | ---: |
| 240 | Nearest / Off | 59.64 | 17.97 | 18.84 |
| 240 | Sharp / Off | 59.72 | 18.08 | 18.78 |
| 240 | Smooth 2x / Off | 59.64 | 17.80 | 18.66 |
| 240 | Nearest / LCD | 59.66 | 18.14 | 20.20 |
| 240 | Nearest / CRT | 59.46 | 18.81 | 22.27 |
| 240 | Smooth 2x / CRT | 59.28 | 18.78 | 19.83 |
| 384 | Nearest / Off | 59.08 | 18.47 | 19.86 |
| 384 | Sharp / Off | 57.70 | 19.55 | 23.43 |
| 384 | Smooth 2x / Off | 43.49 | 27.44 | 31.56 |
| 384 | Nearest / LCD | 58.82 | 19.26 | 20.58 |
| 384 | Nearest / CRT | 56.67 | 20.88 | 23.05 |
| 384 | Smooth 2x / CRT | 45.50 | 23.97 | 24.91 |

All twelve runs reported zero fallback frames. These are short developer-PC
smoke measurements, not hardware-independent guarantees or a statistically
controlled ranking of small differences. At exact 3x Sharp deliberately uses
its direct nearest path; fractional Sharp is separately exercised by the
integration test, not measured by this timing matrix.

Smooth 2x is CPU-heavy and measurably slows widescreen combat on this PC. This
limitation is stated in launcher/runtime help and both portable README variants.
Use Nearest or Sharp with effects Off when frame rate matters. Optimization to
guarantee full-speed widescreen smoothing is not claimed complete.

## Scope and release status

Reusable rendering/preferences live in gbarecomp; reusable launcher controls
live in recomp-ui; the Swordcraft adapter opts into these capabilities. No
third-party dependency was added. Existing player data is never used as a
package source. No GitHub publication, commit, tag or release upload is part of
this change. Existing binary/artwork/notice review remains open as documented in
packaging/portable-beta/RELEASE-REVIEW.md.

## Packaged and installed result

Candidate: owner-project
release/Portable Release Candidates/20260928T222902Z-fed6141e/
Swordcraft-Story-3-Portable-Beta-Windows-x64-CANDIDATE.zip.

- ZIP size: 75,970,015 bytes; 53 allowlisted payload files.
- ZIP SHA-256: 4fe90edd7d20fe08ee2780e362c0c1434e8bc484f2c08819a952ac9784ddb3c5.
- ZIP verifier, dependency closure, debug-strip loaded-section checks and five
  ROM-free packaging-verifier tests passed. Player-data folders start empty.
- Exact extracted ZIP in validation/pkg-fed6141e passed all six launcher pages,
  three window sizes, credits/tools navigation, keyboard binding persistence and
  eleven artwork rotations. Tests used the system-only PATH and unrelated cwd.
- Separate validation/filter-release-first-run-20260928-fed6141e extraction
  passed read-only preflight, empty-input startup and normal close; no developer
  ROM, BIOS or saves were discovered and no packaged content was changed.
- These two launcher checks must be sequential: an initial simultaneous run
  hit the existing global single-instance guard and lacked the second session
  log. The isolated full page/artwork rerun passed. No game-code fix was needed.
- Installed the candidate's two engines and top-level starter into the owner's
  existing release/Portable Beta, plus its updated player README. Installer
  preflight passed. All 16 existing files in ROMs, BIOS, Mods, Saves, Save States,
  Settings and Credits were hash-checked unchanged.
- Previous program files and README are backed up under the owner project's
  release/Language Support Rollback 20260928T223321567, with UPDATE-REPORT.json.
  No previous package was deleted. No user game process was stopped.

Installed English runtime SHA-256:
50b3d71b702cdc5b6ada1160a6aa308b54ebd7c2292aa43ff7d3cbff061f93d1.

Installed Japanese runtime SHA-256:
ee537fb83f3032da9ca2bd28d7f2e580b428a1de65c75fff9dd7176ce59535d5.
