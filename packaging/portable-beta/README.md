# Swordcraft Story 3 — Portable Beta

Windows x64. Portable UI test build; publication review is still open.

This build includes the reorganized launcher and Esc menu. Please test it in a
new folder and report your Windows version, selected language, graphics settings,
controller, and steps to reproduce any problem.

## Start here

1. Extract the **entire ZIP** into a new writable folder.
2. Open **Swordcraft Story 3 Beta.exe**. Keep the Runtime folder beside it.
   No BAT file, installer, or development tools are needed for the launcher.
3. Select your own Japanese game ROM and GBA BIOS. Imported copies keep their
   original filenames; originals remain unchanged. Different files with the
   same name get a readable suffix such as `Game (2).gba`.
4. **Japanese needs no translation patch.** For English, optionally select your
   released **Hajimari_no_Ishi_v1.0.6.f.bps** in **Mods** and enable it. An already-translated
   ROM matching this build can also be selected directly.
5. Press **Play**. Press **Esc** during play for the in-game menu.

Do not run from inside the ZIP. Avoid Program Files and other read-only folders.
Move the whole extracted folder when moving to another computer.
Use a reasonably short folder path (for example `C:\Games\Swordcraft3`). Very
deeply nested paths can exceed Windows file limits when creating patch caches.

ROMs, BIOS, translation patches, saves, settings and captures are NOT included.
You must supply the required inputs. The launcher is not the game data.

## Portable folders

| Folder | Purpose |
| --- | --- |
| Runtime | Game program, required libraries, artwork, fonts and notices |
| Credits | Editable original-game, PC-port and tools credits |
| ROMs / BIOS | Locally imported game inputs |
| Mods | User-supplied patches and locally generated patched-ROM cache |
| Saves | In-game battery save and import/clear backups |
| Save States | Separate state slots |
| Settings | Controls, launcher preferences and file selections |
| Captures / Logs | F10 captures, session recordings and runtime diagnostics |

Game-owned data stays beside the launcher; there is no AppData fallback.
Windows and drivers can still maintain their own operating-system data.
Player-data folders start empty. The patcher's internal cache still uses verified
content identities; the selected ROM's display name is separate from that cache.

## Controls and saves

Keyboard and gamepad work together. Change bindings in **Controls**. Defaults:
arrows move; X = A; Z = B; C = L; V = R; Enter = Start; right Shift = Select.
Rewind/fast-forward controls are under **Assist Tools**.

Use the game's save command for a normal battery save. Home > Import copies an
existing battery save into this installation; it does not use the external file
in place. Save states and rewind are separate features.

Home shows the selected game language; expand **Game files** for full paths and
BIOS options. Japanese uses
`Saves/japanese.eep` and `Save States/japanese.state1` through `.state10`.
Released English uses `Saves/english-1.0.6f.eep` and
`Save States/english-1.0.6f.state1` through `.state10`.
Earlier released-English saves remain under their versioned names; save states
are bound to their exact ROM and are not automatically converted on upgrade.
Old beta `battery.eep` and `beta.state*` files are preserved, not automatically
loaded or converted. Do not use old beta save states with the released patch.
Battery-save compatibility has not been established; keep a backup and use
Import only if you deliberately want to test one. Switching language does not overwrite saves.
Select the intended language before importing a battery save. Do not exchange
save states between languages. The launcher interface itself remains in English.

With the original Japanese ROM selected, disable the patch in **Mods** to play
Japanese again. Disabling a patch cannot undo an already-translated ROM: select
the original Japanese ROM with **Change ROM** in that case.

This package no longer runs the retired English beta translation. The internal
engine filename still contains `Beta` for starter compatibility; its English
code and ROM verification are for release **1.0.6.f**.

The Esc menu offers **Pause**, **Resume**, **Reset game** and **Quit game**.
Choose whether opening the menu automatically pauses play. Reset/Close ask for
confirmation; unsaved progress can be lost. State compatibility across future
builds is not guaranteed, so keep ordinary in-game saves too.

The bottom **Resume** button resumes gameplay even after a manual pause.
Controls and Mods are configured in the launcher, not inside Esc.

## Optional dedicated Guard

Open **Mods > Gameplay Mods > Hold Select to Guard** in the normal launcher.
With it on, hold your mapped Select button during manual combat to guard and
release it to stop, using the game's native Guard timing. Your selected R-slot
ability is preserved. Select can still cancel existing auto-battle, but does not
turn it on while this mod is enabled. Outside combat, Select is unchanged.
Turn the option off for the original controls, including the auto-battle toggle.

The choice saves immediately in **Settings/guard.ini**, applies when you press
Play, and survives reopening, Reset Game and switching between Japanese and
English. Fresh installations default to Off. No patch download or separate
Guard Test launcher is needed. Change Select's keyboard/gamepad bindings in
**Controls**. A save failure leaves the previous choice active and shows an error.

## Visual filters

### Optional battle framing experiment

Use **Graphics > Battle View > Battle framing** in the launcher or Esc.
Choose **Current**, **Bounded**, or **Follow + edge stops**. **Current** is the
default and restores the existing centered widescreen presentation.

**Bounded view** shifts the captured gameplay view inward at both ends of the
original camera's logical range. It keeps the HUD centered and never stretches
or zooms the characters. At 12:5, reviewed arenas use 368 gameplay columns with
an 8-column warm brown-and-gold border with an inset shadow on each side. This
decoration stays outside the scenery and never shades fighters or the HUD.
At 16:9 it follows within the bounds without adding a border over scenery. Original
GBA mode, field maps and unsupported arenas retain their current presentation.
Locked camera sequences and out-of-range camera shake also retain the current view.

**Follow + edge stops** keeps the original centered widescreen framing away
from the edges, then shifts all gameplay layers together to stay inside the
near-scenery source bounds on both sides. At 12:5 it uses a fixed 346-column
opening with a 19-column border/shadow frame on each side. Characters are not
stretched or zoomed. At 16:9 it uses the whole 284-column opening. This does not
invent missing art or guarantee that intentionally transparent scenery is filled.
Both experiments cover reviewed arenas 0, 2, 3 and 7; other arenas retain their
existing fallback. Locked/special scenes and out-of-envelope raster schedules
retain Current framing.

These options are mutually exclusive. Choose **Current** to restore the
original view. **Cover scenery edges** requires **Follow + edge stops** and
covers the reviewed rocky arena's scenery cutoffs; other arenas are unchanged.

This is a framing experiment, not a rewrite of the original perspective camera:
its parallax and perspective movement are retained. The setting applies on
Resume, survives reopening/reset, and lives in `Settings/battle-camera.ini`.
Toggle it off at any time if you prefer the previous view.

### Filter presets

Start with **Graphics preset** under **Graphics > Picture** in the launcher or Esc.
You do not need to configure each filter separately:

| Preset | Look | Combination |
| --- | --- | --- |
| Original Pixels | Hard-edged pixels; simplest and lightest | Nearest, effects Off |
| Clean & Crisp | Recommended starting point for resized windows | Sharp fractional, effects Off |
| Soft & Smooth | Rounded edges; lettering may look softer | Smooth 2x, effects Off |
| Handheld Grid | A light handheld-style pixel grid | Nearest, LCD Grid 25% |
| Retro TV | Softened pixels with scanlines and edge shading | Linear, CRT 35% |

These are filter recipes, not performance/quality tiers. They leave your
aspect ratio, screen-colour model, window size, audio and controls unchanged.
They do not force an exact hardware emulation. Your existing choices stay
active until you choose a preset. Fine-tune the individual controls below it;
the label automatically becomes **Custom** when the combination differs.
Choosing a named preset again reapplies its recipe. Changes survive Reset Game
and reopening the launcher. There is no extra preset file to manage.

**Aspect ratio** is available under **Graphics > Window & Screen** in the launcher or Esc:
Original GBA (3:2), Widescreen (16:9), or Ultrawide (12:5). Switch during play;
when paused, the new view takes effect on Resume. The choice also survives
Reset Game and reopening the launcher. Existing installations keep their
12:5 default until a new choice is saved.

These change the rendered view, not horizontal stretching. The renderer uses
240, 284, or 384 columns at 160 rows; 284 is the pixel-aligned 16:9 approximation.
The game's native simulation and camera remain unchanged. Unsupported scenes
keep their native framing. Changing aspect does not resize the desktop window;
unused space is letterboxed. Window scale/fullscreen remain separate controls.

Choose filters under **Graphics > Picture** in the launcher or Esc.
Scaling offers **Nearest**, **Linear**, **Sharp fractional**, and **Smooth 2x**.
Screen effects offer **Off**, **LCD Grid**, and **CRT**, with adjustable intensity.
The existing screen-colour presets can be combined with these options.

**Colour profile** is available under **Graphics > Picture** in the launcher or
Esc: Raw, Unlit, Frontlit, Backlit and Classic. This changes game
colours immediately, including while paused; launcher/Esc text stays unchanged.
It is independent of scaling, effects, graphics presets and aspect ratio.
The choice is saved in Settings/launcher.ini and survives Reset Game and
reopening the launcher. If it cannot be saved, the previous model stays active.

The original pixel look is still the default. Smooth 2x is a custom edge-aware
filter, not xBRZ; it can soften lettering. Its CPU path is now optimized for
widescreen combat; performance still depends on your hardware. CRT is a
lightweight scanline/grille/vignette effect. Filters leave launcher/Esc text
unfiltered and do not alter saves or game logic. Choices are kept in
Settings/launcher.ini, including changes made before Reset Game.

If frame rate suffers, choose Nearest or Sharp with screen effects Off.

## Compatibility

This candidate adds separate Japanese/English engines and readable imported
filenames to the prerelease06 checkpoint. The launcher selects the engine by
verified ROM contents, not its filename; the translation is optional.
It includes the themed launcher/Esc menu and experimental 12:5 presentation.
It is not a 1.0 release. Unsupported scenes retain their documented fallback.
Not every game route, spell, controller or clean-machine configuration is tested.
Historical chest/rewind failures are not claimed fixed by this package.

Required input SHA-1 identities:

- Japanese ROM: `3f5253fcf57e07ce52472bd29a61d16b98a12376`
- GBA BIOS: `300c20df6731a33952ded8c436f7f186d25d3492`
- Translated output (1.0.6.f): `6753a22a096b8adaa3a869333b99fcfe29ba1fec`

A newer translation may require a new executable. No compiler or self-healing
cache is included; untested dynamic-code paths remain a limitation.

For an existing installation, extract into a **new folder** and back up saves
first. Import your battery save through the launcher. Do not overwrite/delete
your old installation or copy old settings blindly.

## Package records

`Runtime/package-manifest.json` records source/build identities.
`Runtime/SHA256SUMS.txt` covers packaged files except itself. These describe the
untouched ZIP; normal play deliberately creates local data.

Read `Runtime/notices/RELEASE-REVIEW.md` before distribution. Included notices
and technical checks are not a complete licensing/provenance clearance.
