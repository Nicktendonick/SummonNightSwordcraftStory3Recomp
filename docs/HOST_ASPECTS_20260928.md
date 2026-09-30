# Portable host aspect ratios — 28 September 2026

## User-facing result

Launcher Settings > Display and Esc > Graphics expose **Game aspect ratio**:
Original GBA (3:2), Widescreen (16:9), Ultrawide (12:5). Host widths are 240, 284,
384 at 160 rows. 284 is the existing integer-column approximation of 16:9.
Changing a paused session is queued until Resume; running-menu changes apply at
the next completed presented frame. No restart is required. The window keeps
its desktop dimensions and letterboxes; its scale/fullscreen controls are separate.

Both menus share `Settings/launcher.ini` / `host_aspect_index`. A missing key
uses the portable launcher's existing 12:5 default, not the legacy hidden PPU
aspect_index=0. Reset's parent supervisor rereads the current host preference
before relaunching. Invalid/duplicate values are rejected rather than becoming 0.
Settings failures reject the live change. Existing media and save paths are untouched.

## Scope and rendering contract

Guest PPU remains 240x160; no camera, collision, timing or new scene policy was
added. Native mode disables the existing wider draw-list adjustments; wider
modes retain their already-established resource/draw-list bookkeeping. Host
width changes invalidate capture, battle/field and spell-window caches at a
frame boundary. The transition may use native-centred fallback until a fresh
complete capture exists. Unsupported scenes keep their native fallback.

Reusable runtime surface sizing, preference helpers and menu items are in
gbarecomp. Reusable launcher wording is in recomp-ui. Game-specific preset
labels/widths and cache invalidation are in src/custom_renderer.cpp. The reset
supervisor's preference refresh is in src/beta_session.h. No generated game C
or reference disassembly was edited, and no Guard gameplay mod was implemented.

## Evidence

- Incremental English/Japanese engines and targeted test executables built.
- New preference, launcher argument and reset tests pass. They cover all three
  values, old-install default, preservation of unrelated keys/BOM, invalid values,
  and host-aspect arguments without widening guest scanout.
- `tools/test_host_aspects.py` passed for both languages, six live transitions
  per session, selected-versus-applied pause state, invalid index, cold Reset,
  launcher reload/edit/reopen and protected-input hashes. Dummy/software run:
  `validation/host-aspects-1790639409904623400`.
- Real SDL window/GPU rerun also passed for both languages:
  `validation/host-aspects-1790639723050475500`. English used the read-only known
  rocky combat state; Japanese boot is the negative/unsupported-scene case.
- Runtime logs show native guest width 240 throughout. Combat ownership traces
  confirm complete 160-row frames at 284 columns (7,040 extended samples) and
  384 columns (23,040 extended samples), in addition to 38,400 native samples.
  These are state/ownership counters, not framebuffer comparisons.
- Paused checks compare frame count, cycles and PC before Resume. They exclude
  the separate reset child's restarted pump counter.
- Of 51 registered CTests, 33 passed on the first broad run; 17 binaries were
  not built in this checkout. The remaining configuration-guard test initially
  lacked Ninja on PATH and passed when rerun with the existing project tool.
  Therefore **34 executed tests passed, 17 were unavailable**, not a full-suite pass.
- The actual launcher screenshot shows the new control in Display. Visual
  inspection is supplemental UI review, not game-state/scene detection.

## Screenshot comparison

`validation/display-guide-20260928/index.html` links 17 actual Windows captures
and explains every Display control. Captures use the unchanged previous filter
build, the same restored combat state/neutral input, and an explicit pause.
They cover all four scalers, Raw plus four colour models, LCD/CRT at 35% and 70%,
all three aspect widths, 2x/3x window sizing, and borderless fullscreen Nearest
versus Sharp. Exclusive fullscreen is explained but not separately captured.
The pause notice remains visible; original captures are unedited. Pixel/visual
comparisons were not used as automated assertions. Source-integrity hashes pass.

The first Windows capture needed approval; a hidden-window capture returned
the foreground app, so it was discarded without saving. Selecting and activating
only the task-owned game window produced the delivered captures. No user game
process was stopped. A temporary staging PNG outside the owner project is removed
after transferring the final capture. The comparison page cannot be opened by
the browser-control tool because its policy blocks file URLs; it is provided as
a local file for the user to open. Browser layout is not claimed verified.

## Package and limits

Candidate: owner-project `release/Portable Release Candidates/20260928T235608Z-f5365721`.
ZIP: `Swordcraft-Story-3-Portable-Beta-Windows-x64-CANDIDATE.zip`.
53 allowlisted payload files, 75,981,531 bytes.
SHA-256: `c1f04d3dfaf1268c410346d1167dddcd8980f60f8f1870d96b21f45a7b66df3d`.
Dependency/loaded-section checks and exact archive verification passed.
The exact extracted package passed the six launcher pages, three window sizes,
keyboard binding persistence, two credits panes/tools, and all eleven artwork
rotations. A second fresh extraction passed read-only preflight, empty-input
first launch and normal close without borrowing developer ROM/BIOS/save data.
Evidence: `validation/pkg-f5365721` and `validation/aspect-first-run-f5365721`.

Installed into the owner's existing `release/Portable Beta`; preflight passed.
All 16 protected files in ROMs, BIOS, Mods, Saves, Save States, Settings and
Credits were hash-checked unchanged. Previous programs and README were backed up
to `release/Language Support Rollback 20260929T000037001/UPDATE-REPORT.json`.
No user process was stopped and no old release was removed.

Installed English engine SHA-256:
`c1570c220a5cda6d62e0cb1415cb06fa05ef5b6b5fd2b11369264e9f08733deb`.
Installed Japanese engine SHA-256:
`cf166934fc41aec1dce5f5d18b9b518cc865d5c3b23422fbe076bc36e48178bd`.
The comparison page passed a static DOM/script check of all 17 original-image
links, every selection and the swap handler; browser layout remains unverified.

This is bounded beta evidence, not all-route gameplay certification. Physical
controller navigation, multi-hour sessions, every spell/transition, and arbitrary
window/monitor combinations remain outside this check. Smooth 2x still has the
documented widescreen-combat CPU cost. No GitHub commit, push or release upload.

## Dedicated Guard button: feasibility only

The current battle-state hook already identifies the battle root and player
attributes; the R-slot cursor is player+0x166. The read-only csm3 reference names
that field cursorPosition and contains control paths that select values 0..5.
This provides a useful research lead, not a verified direct Guard entry point.
Neither Guard's exact selector/action mapping nor a new cross-language action
hook was validated in this task.

Recommended future design: optional, separately bindable **hold Guard**, leaving
the selected R-slot intact after release. It should request the game's ordinary
block action only when normally legal, retaining recovery/hitstun/airborne rules
and costs. A native action hook is preferable to a fixed R-tapping macro, which
depends on the current slot. A temporary select/restore prototype is possible
but must handle death, menus, focus loss, rewind/save restoration and battle exit.
Validate both ROM variants and keyboard/gamepad before promoting it. No automatic
guard, invulnerability or gameplay change was added here.
