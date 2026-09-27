# Portable launcher validation — 2026-09-24

## Scope and storage contract

Functional launcher fixes, before cosmetic re-theming. No renderer or guest
simulation changes were made as part of this launcher work. Existing dirty
renderer/performance changes in this checkout were preserved.

The native starter anchors game-owned data to its own executable directory.
Settings, Saves, Save States, ROMs, BIOS, Mods, Captures and Logs are visible
local folders. Packaged runtime code and artwork live under Runtime. Failure
to write locally is an error, not permission to fall back to AppData. Windows
and drivers may still maintain their own data outside this directory.

The development starter copies legacy saves only when each destination is
absent, retaining the originals. Future play uses the portable copies. The
standalone packager uses an allowlist and requires a new destination; it does
not include a ROM, BIOS, translation patch or player save.

## Functional changes

- Battery panel names the active file; explicit launcher and runtime save paths
  agree. Import validates the expected size. Import and Clear preserve numbered
  backups. Save states are separate from the battery save.
- Hybrid keyboard/gamepad bindings use the same persisted settings as the
  runtime, including normal launcher close. Actual keyboard and controller
  capture routes now update those settings instead of an unused legacy file.
- Assist bindings are separate from gameplay Controls. The launcher documents
  the existing ten state slots and in-game menu. F10 capture no longer requires
  the visible debugger that intercepted state-slot function keys.
- Mods accepts the source ROM plus a compatible patch, or the exact prepared
  beta ROM without double-patching. Output identity is checked; arbitrary future
  translation versions are not automatically compatible with this executable.
- Imported files stay local and persisted selections are relative. Cover art
  alternation remains intact. No new gameplay cheats were added.

## Repository boundaries

Game checkout: experiment/full-viewport-renderer-20260923.
Both gbarecomp and recomp-ui: feature/portable-hybrid-launcher-20260924.
Reusable import/storage/input/UI work lives in those respective submodules.
These are uncommitted local changes; this task did not push or publish a release.

## Evidence

- Latest build completed. Three targeted CTests passed:
  swordcraft3_portable_launcher_tests, swordcraft3_portable_input_tests,
  swordcraft3_beta_boxart_tests. This is not a run of every registered test.
- Synthetic save tests cover invalid import size, non-overwrite, backups,
  Clear and a failing destination. They do not clear a player's save.
- Actual SDL runtime input test used a virtual controller with physical
  controller backends disabled only in that test process. Remapped input and
  fast-forward trigger behavior passed without a ROM or gameplay execution.
- Private integration test applied the user's actual beta BPS to the original
  ROM and verified the required target; an incompatible IPS was rejected.
- Actual launcher UI automation rebound a key, closed normally, moved the test
  package, and reopened it with the binding retained. Both launches used a
  system-only PATH and unrelated working directory. Original ROM/BIOS hashes
  were unchanged. Latest test folder:
  validation/portable-final-20260924-213613-moved-moved.
- Development starter smoke test verified normal cancellation, alternating
  cover artwork, read-only preflight and seven original save files unchanged,
  with matching portable copies.
- git diff --check passed in game, engine and UI repositories (existing line
  ending warnings remain). Release candidate was checked for private game
  inputs and save extensions; none were packaged.

Final starter SHA256:
4B57F22720732FADE52239DB96F107C80D416DC897A7E532A7BAC556CFFCE682

Final runtime SHA256:
F4F5E30AA8A6860A6E3DA0042E6756FF370270339550E78C3E4564C8A4A21490

### Follow-up: missing Mods navigation

The first portable build enabled patch import but still gated the dashboard
Mods button on the optional mod-catalog provider. With RECOMP_UI_ENABLE_MODS=OFF,
the provider was null and the button was absent. The page itself already
supported patch-only operation. Earlier patch tests missed this navigation gate.

A shared launcher_model_has_mods_view predicate now governs both dashboard
button visibility/layout and the Mods page. It accepts either a catalog provider
or ROM patch support. Regression tests cover neither, patch-only and catalog-only
configurations plus navigation to the Mods view. All three targeted CTests,
the actual BPS integration and development launcher smoke test passed again.
Seven original saves and their portable copies remain unchanged. The release
candidate runtime was refreshed. No gameplay code changed in this correction.

## Remaining acceptance

### Credits addition

The game opts into two independently scrolling, read-only credits panels in
recomp-ui, leaving other games' single-panel credits behavior intact. The game
loads UTF-8 text from Credits/original-game.txt and Credits/pc-port.txt beside
the launcher. Missing/empty files retain Pending A / Pending B fallbacks.
The original-game file now contains the roles/names pasted by the owner from
MobyGames, with source attribution; names were not independently re-verified.
PC Port Credits initially used Pending B and now uses the owner's supplied text.
Package/build scripts seed editable files
without overwriting customized credits in the development installation.

Noto Sans JP and its OFL license ship locally; the UI builds a glyph range
including the supplied names, not only the standard Japanese subset. Font
SHA256: C2F3B4D463500A2DDCD3849CDED1FCEEB9FD6D1C32E6CBECD568453BA50FC68F.
Tests covered missing/empty files, UTF-8 BOM, percent signs, 500 lines, model
text/title propagation and Credits navigation. The three targeted CTests passed.
tools/test_beta_credits.ps1 opened/reopened the real Credits page in a test
package and checked atlas glyph coverage: two panels, zero missing glyphs.
No pixel assertions were used. Manual scrolling/layout acceptance is still
requested. Both installed launchers were updated; save-integrity smoke passed.

### Outstanding acceptance

### Tools and open-source projects detail page

Credits now offers an opt-in Tools & Open-Source Projects button opening a
scrollable detail page with Back to Credits. The game supplies its plain UTF-8
content from Credits/tools-and-projects.txt. Standalone HTTP(S) URL lines render
as user-activated links; no website opens merely by viewing the page. Categories
separate core tools, development tools, architectural references and supporting
libraries/fonts. This acknowledgment is not an exhaustive license inventory.

The reusable text/model/navigation UI changes live in recomp-ui; runtime options
and forwarding live in gbarecomp. Game text and file loading remain game-owned.
Tests covered opt-in text propagation, open/close, leaving Credits, missing text
and inappropriate entry from another page. All three targeted CTests passed.
The portable smoke opened/closed the detail view using the same model action as
the button and checked glyph coverage for all three files (zero missing glyphs).
It did not click external links or use pixel assertions. Personal credit files
are preserved during deployment. No gameplay renderer changes were made.

### Remaining player checks

Physical-controller usability and final layout need player acceptance. No new
gameplay playthrough, rendering comparison or performance benchmark is claimed.
The package must be moved as a whole; a lone starter EXE is not a portable game.
Use a writable folder. The clean release candidate deliberately starts without
the player's saves; the usual project starter retains the migrated progress.
