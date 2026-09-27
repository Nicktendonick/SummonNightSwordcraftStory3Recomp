# Native storybook launcher — 2026-09-26

This is the actual recomp-ui launcher, not the browser prototype.
The change is presentation-only. Game rendering, ROM hooks, timings, audio,
save formats, patch compatibility, and input encoding are unchanged.

## Layout

- Cream paper, plum headings, crystal-blue actions; no scanline overlay.
- Supplied English logo and village header. Only the approved quote:
  "Craftknights! Artisans of weapons. Masters of the sword."
- Persistent Home / Settings / Controls / Assist Tools / Mods / Credits navigation.
- Home displays full, aspect-preserved artwork alongside the existing real
  game-file, save import/backup/clear, and BIOS controls.
- Settings retains all profile-provided sections in a scrollable single column.
  The game-folder explanation lives here, not in the header or Home.
- Existing hybrid keyboard/gamepad controls, BPS patch interface, both credit
  panels and Tools & Projects details reuse their established implementations.
- No "Portable" badge or invented inspirational slogans.

## Artwork and customization

`src/beta_boxart.h` owns an allowlisted 11-image cycle. `Settings/boxart.txt`
stores the last token, never an arbitrary image path. The existing `clean`
selection advances to the new archer poster. Invalid/missing state safely
starts at `clean`. It advances on launcher startup, not during gameplay.

Images are ordinary PNGs in `Runtime/assets/beta` in the portable package;
replacing one under the same filename changes its appearance on the next open
without rebuilding. Full artwork aspect ratio is preserved, including the
older landscape covers. Missing artwork falls back to the existing placeholder;
a missing header logo falls back to the game name. Credits remain editable TXT.

Colors, typography and spacing are centralized in
`recomp-ui/src/common/launcher_theme.h::launcher_theme_storybook`.
Native layout is in `launcher_imgui.cpp`. Those code changes currently require
a rebuild; the HTML preview's CSS is not consumed by the native executable.
The quote and game-owned asset paths live in `src/main.cpp`.

The runtime/launcher appearance fields are additive and opt-in; other games
and the normal non-beta command-line paths retain their existing theme/layout.
See `assets/beta/ARTWORK.md` for provenance/distribution caution.

## Verification

- Build targets: `Swordcraft3BetaLauncher`, `Swordcraft3CustomRendererBeta`.
- Regression targets: `swordcraft3_beta_boxart_tests`,
  `swordcraft3_portable_launcher_tests`, `swordcraft3_portable_input_tests`.
- `tools/test_beta_theme.ps1 -Package <validation package>` checks actual native
  startup/close, all 11 decoded assets, the six page states, window-size changes
  through 1240x880, 960x720 and 800x600, credits glyph coverage and tools navigation, real keyboard
  rebinding/persistence, and unchanged save/ROM/BIOS/credit hashes.
- Test evidence uses UI/model state and logs, never screenshot/pixel assertions.
  This is a first playable theme for the owner's aesthetic feedback, not a
  claim of final visual approval or a full playthrough.

## Rollback

The original starter, runtime executable and assets are retained at the project
root under `release/Launcher Rollback 20260926`. Close the launcher/game before
restoring. Its README describes the exact files to copy back. Do not replace
Settings, Saves, Save States, ROMs, BIOS, Mods or Credits when rolling back.

## Completed local validation

Final native build and all three targeted CTest cases passed. The isolated
package smoke test passed all 11 startup rotations, all six page states,
three window sizes, credits glyph/tool checks and an H-key remap that persisted
through subsequent openings. Its saved game files and inputs were unchanged.
The log picker now sorts matching process IDs by time so Windows PID reuse
cannot accidentally select a previous day's capture.

Both the developer starter and Portable Beta were updated. Installation checked
23 protected files across their Settings, Saves, Save States, Credits, ROMs,
BIOS and Mods folders: all hashes were unchanged. The live Portable Beta
launcher window was then opened for the owner; no gameplay was auto-started.

SHA-256 of delivered executables:

- Runtime: `A422B362213F3BE1437B49239540794169EC3BF58ED60E3E2A5EF804856F7EFA`
- Starter: `DFF21EFC88E3A565976B6346D0EAA0F3C6D52035C47AA79FF47463C242F25967`

No commit, push or public release was made in this pass.
