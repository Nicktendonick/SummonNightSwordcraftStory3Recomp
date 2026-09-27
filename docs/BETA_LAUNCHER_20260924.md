# Native beta launcher preview — 2026-09-24

**Historical initial preview.** Its storage and input contract below has been
superseded by [the portable beta](PORTABLE_BETA.md): local folders, verified
patch import, shared hybrid input settings and one-time non-destructive save copying.
The original notes are retained as the record of the first preview, not current
instructions. Use PORTABLE_BETA.md for today's launcher.

Double-click **Swordcraft Story 3 Beta.exe** in the owning Documents project
folder. No batch file, PowerShell window or terminal is needed by the player.
This opens the existing recomp-ui launcher first; Play starts the current
complete-frame overworld/combat beta with the selective performance backport.

## Scope

This is a local launcher/UI review entry point, not a standalone distributable
release. Keep the executable in the project folder: it locates the existing game
under `experiments/full-viewport-renderer/build-native`. It requires the already
prepared private English-beta ROM, BIOS, runtime DLLs and launcher assets.
It neither downloads dependencies nor modifies the supplied ROM, BIOS or patch.

The owner-supplied PNG is preserved and converted into seven Windows icon sizes.
The icon is embedded in both the starter and the game executable; the UI's internal
artwork/theme is not redesigned in this pass. See `assets/beta/README.md` for origin
and source hash. No image-generation or artwork alteration was used.

All code here is game-owned. No new gbarecomp/recomp-ui changes were needed. Their
pre-existing engine performance delta is retained. Gameplay renderer/hook logic
is unchanged. Existing batch launchers and pre-performance rollback are preserved.

## Launch contract

- Same 384x160 (12:5) complete field/combat and expanded-object switches as the
  current full-frame test; existing unsupported-scene fallback is unchanged.
- Clears inherited experimental game/runtime/SDL environment variables only in
  the starter process before constructing the child's settings. No global changes.
- Uses `build-native/full-combat-playtest/native-renderer.eep` and the existing
  private ROM's state slots; no save migration or copying occurs.
- Does not pass `--rom`: the current engine launcher seam treats that argument as
  headless/bypass even with `--launcher`. Instead it seeds separate first-run
  `beta-launcher-rom.cfg` and `beta-launcher-bios.cfg` beside the game executable.
- `SWORDCRAFT3_BETA_LAUNCHER=1` selects game-owned UI-preview options: beta branding,
  verification against the already prepared beta ROM, and no patch-import panel.
  The legacy stock-ROM-plus-patch launcher remains unchanged outside this mode.
- Preview settings use `config-beta-preview.ini` and `keybinds-beta-preview.ini`.
  These start separately from older settings. Battle HUD borders retain their
  existing `swordcraft3-display.ini` storage. Existing settings files are not reset.
- No console window. Session logs, F10 captures and input recordings go under
  `validation/beta-launcher/<timestamp-pid>/`. A nonzero exit gives a log-location
  message. Normal cancellation returns success.
- A named instance guard prevents two copies of this starter running together.
  It does not detect games launched through older batch files; close those first.

## UI review checklist / known limits

Please review home screen, settings, controller/keyboard binding, Assist Tools,
fast-forward speed, audio, fullscreen/window scale, and Play/close/reopen behavior.
Verify that committed preferences survive reopening and that your current saves
appear in-game. Launcher creation/cancellation is tested; all interactive settings
and clicking Play have not yet been owner-reviewed through this new entry point.

The custom-renderer viewport is still fixed to the tested **12:5** in this preview.
Its configuration deliberately hides legacy adaptive/aspect controls. A working
custom-renderer aspect selector, final theme/branding/layout, patch import for a
clean installation, and release packaging remain separate UI/release work. Do not
present this starter alone as a portable game or claim the whole UI is certified.

## Build and evidence

After preparing the existing private build, `tools/build_beta_launcher.ps1`
configures/builds the two targets and deploys the starter to the owner folder.
It refuses to replace an unrelated file with the same name. Icon regeneration:
`tools/prepare_beta_icon.ps1 -Source <owner-supplied PNG>`.

- Starter SHA-256: `0ad90f99fdd786a00a0cdd559cd2d7aab059a7e4ff9c846dfbb6f5a0f70bd37d`.
- Game SHA-256: `1b844f116118d9566ade14e82b1b6c2b15efe8f82b838d519231a7bd7ee49543`.
- Starter imports only KERNEL32, USER32 and msvcrt; MinGW C++ runtime is statically
  linked into the starter. The separate game retains its staged DLL requirements.
- `tools/test_beta_launcher.ps1` passed with system-only PATH and an unrelated
  working directory: observed the actual `PC Beta — Launcher` window, requested
  normal cancellation, verified successful exit and seven unchanged save hashes.
  This is process/state evidence, not screenshot matching or complete UI inspection.
- All 22 selected source/state/config/audio/codegen tests passed again.
- 148 paired frames (lake movement and combat R/jump) passed with the beta-preview
  option enabled on the new binary: sampled guest state/cycles, field eligibility
  and full-frame ownership unchanged. Evidence: `validation/beta-launcher/paired/`.

No commits, push, release publication, or repository visibility changes in this
pass. Asset distribution review remains a release gate.

## Follow-up: alternating box art

The owner supplied two cover images and selected **switch each time the launcher
opens**, not a timed slideshow. The native starter now alternates clean artwork
and original Japanese cover. It stores the last selection in the separate
`build-native/beta-boxart-state.txt`; missing/invalid state starts with clean art.
Preflight, duplicate-instance rejection and CreateProcess failure do not advance
the selection. Successful child creation persists the choice, including a session
subsequently cancelled without pressing Play. This is not a gameplay setting.

The existing recomp-ui `launcher_boxart` interface loads the staged PNG selected
by a fixed game-owned token/path mapping. Its existing fit-to-panel code preserves
the full image and aspect ratio. No cropping, artwork edits, generic UI changes,
icon changes, rendering changes, or save migration were made.

Current follow-up binary identities (supersede the initial launcher hashes above):

- Starter: `ecb7f31c05f3d2a85ffe6a6b21966aab63152954481b0f0f29e73d8d3e8f3ec5`.
- Game: `a7fec84976da44a9e1ba6dc595294768ef987706db59f70acfbe5df651bea0c4`.

Verification: dedicated box-art test covers 100 alternating selections, default
and invalid preference, and fixed paths. Two actual launcher open/cancel smoke
tests selected clean then original as confirmed by the game UI-option log; both
exited normally with seven save hashes unchanged. Source/staged PNG hashes match.
The earlier 22-unit/148-frame results above describe the initial launcher build;
this artwork-only follow-up did not rerun gameplay or use screenshot assertions.
