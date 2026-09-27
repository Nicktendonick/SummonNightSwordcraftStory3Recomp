# Portable beta source checkpoint — 2026-09-26

This checkpoint collects the locally tested September 24–26 progress on
`experiment/full-viewport-renderer-20260923`. It is not a 1.0 release, a binary
distribution, or a claim that all game routes have been tested.

## Included work

- Complete-frame custom field composition alongside complete-frame combat,
  retaining the documented source/scene guards and rollback launchers.
- Selective opt-in diagnostic/performance backport. This is not a wholesale
  upstream engine or UI upgrade; compiler/overlay migration remains deferred.
- Native portable beta starter and themed launcher, custom icon, eleven
  startup-alternating artwork choices, supplied logo and village header.
- Local file import and battery-save management, remembered hybrid keyboard
  and gamepad controls, separate Assist Tools, and translation-patch selection.
- Editable Original Game / PC Port credit panels and Tools & Projects details.
- Themed Esc menu with adjacent Pause/Resume actions, a persistent automatic
  menu-pause preference, and Cancel-first Reset/Close confirmation. Manual Pause
  stays active until Resume. Reset uses a clean child process with the same
  resolved game inputs after successful save flush.

Reusable UI changes belong to `recomp-ui`; reusable runtime changes belong to
`gbarecomp`. Both use `feature/portable-hybrid-launcher-20260924` in the owner's
private repositories. The game records their exact commits as submodule pins.
Existing default branches and repository visibility are not changed.

- Engine: `10bef03cfae0408bb72792f176a1600ac5911494`.
- UI: `2fed541538b69cc710e0b18a8afea11dfc5f8c4a`.

## Portable deployment verified

The root developer starter and `release/Portable Beta` use identical current
starter/runtime binaries. All eleven deployed artwork choices and the logo,
village header, font and license match their source assets. Deployed credit text
matches the templates (the PC Port file differs only in its final newline).
Build-time icon source/resource files and source README files are not runtime
requirements.

- Runtime SHA-256:
  `93C0F1775A7EC7ED30CEEAFEE1239AE0D45B1EB76B052A69BA78B90D17942694`
- Starter SHA-256:
  `DFF21EFC88E3A565976B6346D0EAA0F3C6D52035C47AA79FF47463C242F25967`

Keep the whole Portable Beta directory together when moving it. Game-owned
settings, saves, imported inputs, patches, credits and captures remain beside
its starter, with no AppData fallback. Windows or drivers may independently
retain their own OS data. See [portable setup](PORTABLE_BETA.md).

## Checkpoint verification

All 22 selected existing CTest cases passed during this publication pass:
19 `swordcraft3_*` cases covering renderer state/source behavior, artwork,
launcher, input and runtime menu, both diagnostic-capture modes, and codegen.
Source/documentation whitespace checks passed. The supplied PC Port credits
and upstream font license retain five existing trailing spaces unchanged.
The pending file list
contains no ROM, BIOS, patch, save, generated guest code, executable, DLL or
private validation output; a targeted credential-pattern scan found no matches.

Earlier behavior tests, deployment checks and their bounded coverage are
documented in [Esc-menu evidence](ESC_MENU_20260926.md),
[launcher-theme evidence](LAUNCHER_THEME_20260926.md), and
[portable validation](PORTABLE_LAUNCHER_VALIDATION_20260924.md).
Those reports retain their historical build identities. This pass did not rerun
a full playthrough, benchmark, or claim universal spell/scene coverage.

## Publication boundaries

Only source, supplied launcher assets, font with its license, tests, tooling and
documentation are included. Player installs and rollback packages remain local
and untouched. No ROM, BIOS, translation patch, save or ROM-derived generated
code is uploaded. The artwork provenance/distribution caution remains in
[`assets/beta/ARTWORK.md`](../assets/beta/ARTWORK.md); this source checkpoint does
not establish a new license or clear the separate binary-release review.

Older documents describing the field migration or launcher work as unstarted
are historical. Out-of-bounds scenery redesign remains deferred, and the
historical chest/rewind issues are not claimed fixed by this checkpoint.
