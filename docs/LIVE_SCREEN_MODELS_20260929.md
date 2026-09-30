# Live Screen model — 2026-09-29

## Outcome and scope

Esc > Graphics now contains Screen model: Raw, Unlit, Frontlit, Backlit and
Classic, matching the launcher. Changes affect only the final game picture.
They apply on the next presentation, including paused presentations, without
advancing the guest. Presets, scaler, effects/strength, aspect ratio, controls,
audio, save data and Guard behavior are independent and unchanged.

This does not change the existing colour mathematics or claim new hardware
accuracy. The custom renderer, supported scenes and native fallback are
untouched. No screenshots, pixel recognition or framebuffer comparisons were
used for assertions.

## Implementation

- gbarecomp/src/runtime/screen_models.h owns canonical tokens, labels and live
  descriptions. The launcher seam uses the same token list.
- HostWindow records the actual startup model, including a developer environment
  override. A live change builds its ColorLut and correctly sized scratch buffer
  once, requests persistence, then commits using nonthrowing swaps. Invalid
  indices, allocation failure and failed persistence retain the previous model.
  Raw returns to passthrough. The lookup table is not rebuilt each frame.
- The colour transform remains before scaling/effects and before drawing the
  unfiltered Esc overlay. Each present uploads the current transformed source;
  Smooth's existing source-content cache therefore observes the new colours.
- presentation_preferences.h extends the atomic, comment/BOM/section-preserving
  writer to single-line strings. The existing integer writer remains a wrapper.
  Only [Launcher] screen is changed by this control; values use the established
  raw/unlit/frontlit/backlit/classic format, not a new setting.
- beta_session.h refreshes --screen from the persisted selection before each
  Reset Game, independently of the five filter keys and host aspect. Missing,
  malformed, unknown or duplicate values do not replace valid resolved arguments.
- Advanced GBARECOMP_SCREEN environment overrides retain their existing startup
  precedence. The normal portable starter clears development overrides, so
  ordinary portable use follows the player's saved selection.

## Evidence

Exact payload and isolated test logs:
validation/graphics-presets-20260930-002426-5d03/

- Both language engines built successfully.
- 20 targeted CTest checks and five portable-package Python tests passed.
- HostWindow tests exercised 180 combinations: five models, four scaling filters,
  three effects and three widths (240/284/384), using actual presentation
  resources and colour-path counters, not pixel comparisons.
- Startup/environment model reporting, invalid values, failed/throwing persistence,
  same-model persistence, returning to Raw, resizing and SDL device resets passed.
- Atomic INI tests cover readable tokens, BOM/comments, unrelated settings,
  Unicode paths, read-only/locked files and malformed/duplicate values.
- Reset tests cover every model, repeated changes, both --screen argument forms,
  malformed-file rejection and preservation of unrelated command arguments.
- The normal staged portable starter exercised all colour choices without
  development-library search paths or private game inputs.
- Real-window English combat and Japanese cold-boot sessions switched through
  every model while paused. Guest frame/cycles/PC stayed fixed; colour-path
  counters advanced for transformed models and stopped for Raw. Resume advanced
  execution. Existing scaler/effect/strength and host aspect remained unchanged.
- Backlit survived Reset Game, reloaded in the launcher, and a launcher change to
  Classic survived reopening. Read-only settings prevented both model and preset
  changes without modifying the active choices or the file.
- Original ROMs, BIOS and the private combat state remained hash-identical.

The initial host test used SDL_setenv for a variable read through this module's
C runtime. On Windows those environments can differ across DLL runtimes. The
test was corrected to use the same C-runtime environment before rerunning; no
game workaround or environment-precedence change was introduced.

This is targeted verification, not a full playthrough, colour-calibration review
or performance benchmark. Japanese combat fixture coverage remains unavailable.

## Portable delivery

The promotion tool verifies that stripping leaves loaded executable sections
unchanged, checks dependency closure and tests the exact staged binaries.
Only the two Runtime engines and README.md are replaced. The main folder is
snapshotted before staging; installation refuses a running or changed target,
keeps verified rollback copies and verifies all other files unchanged.

PREPARED.json, SMOKE.json and INSTALLED.json in the evidence folder record hashes,
test results and the exact rollback location. No GitHub publication or new
distribution ZIP is included in this change.
