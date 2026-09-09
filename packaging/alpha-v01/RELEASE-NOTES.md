# Alpha unfinished-build v.01

**Draft prerelease notes -- local review only; not published.**

An unfinished Windows x64 development snapshot of the Summon Night:
Swordcraft Story 3 PC port. This candidate uses the existing English-beta
executable from the `capstone-pre-astra-2026-09-07` checkpoint. See the generated
manifest for exact executable hashes and game/framework/UI revisions. No new
gameplay or rendering changes were made for this package.

## Included features

- recomp-ui launcher, keyboard/controller configuration and runtime menu.
- Resizable window, title-bar FPS counter and native GBA frame pacing
  (approximately 59.7275 FPS, not an exact 60 FPS target).
- Experimental Native, 16:9, 2:1, 12:5/full-arena and Adaptive display choices.
- Reviewed overworld map/sprite extensions and experimental battle scenery
  extension. Unsupported scenes may fall back to black margins.
- Optional Assist Tools: 10 save-state slots, rewind history, configurable
  fast-forward controls and a 2x--10x requested-speed slider.
- IPS/IPS32/BPS patch selection. This binary specifically requires the matching
  English-beta output; code-changing patches need matching generated code.

The supplied archive contains one English-beta executable, four runtime DLLs,
ten launcher assets, a click-to-run launcher, instructions, available license
notices and a hash manifest. It contains no separate ROM, BIOS, translation
patch, generated source/object, save, capture or developer configuration file.
The executable itself contains recompiled game and BIOS code; distribution
review is still open, as detailed in `LICENSE-REVIEW.md`.

## Known limitations

- This is not a completed port or an all-game playability certification.
- Later arenas, bosses, menus, transitions, live resizing and extended
  audio/frame-pacing behavior need more testing.
- English-beta routes encounter dynamic-IWRAM execution gaps. Interpreter /
  self-healing fallback is still involved; this is not a fully static build.
  There is no compiler or prepopulated code cache in this candidate.
- Available deterministic battle evidence covers five aligned frames in the
  reviewed village capture, with identical Native/Wide centers and guest state.
  That route is diagnostic, not release-eligible, because of fallback execution.
- Fast-forward multipliers are requested speed, not a performance guarantee.
- Save states depend on ROM identity and snapshot format. Use normal in-game
  saves and back them up; do not rely on state compatibility across versions.
- The proposed tan top/bottom battle borders are NOT implemented here.
- Clean-machine interactive playback and the complete distribution/provenance
  review remain release gates. Automated package checks are not substitutes.

## Reporting problems

Record the release title and executable hash, Windows version, display mode,
scene/location, translation output hash, whether Assist Tools were used, and
steps to reproduce. Keep ROMs, BIOS, patches, saves and raw captures private;
do not attach them to a public issue or release.

## Publication settings after review

- Title: **Alpha unfinished-build v.01**
- Proposed tag: `alpha-unfinished-build-v.01`
- Mark **This is a pre-release**; do not promote it to Latest/stable.
- Keep the existing repository visibility. A release in a private repository
  does not make the project public.
- Obtain the owner's final artifact-list approval before upload/publish.
  The local packager does not contact GitHub or create tags/releases.
