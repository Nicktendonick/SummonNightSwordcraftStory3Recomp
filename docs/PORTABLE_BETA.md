# Portable beta launcher

Open **Swordcraft Story 3 Beta.exe**. Keep the entire folder together.

For the complete current player instructions, use the
[portable package guide](../packaging/portable-beta/README.md). The README inside
your downloaded ZIP describes that particular build. This page summarizes the
Japanese + English **1.1.0f** candidate; older checkpoint documents are historical.

| Folder beside the launcher | Contents |
| --- | --- |
| Runtime | Japanese/English game engines, required DLLs and launcher artwork/fonts |
| Credits | Editable original-game and PC-port credit text files |
| Settings | Launcher controls, display preferences, relative file selections and cover rotation |
| Saves | English: english-1.1.0f.eep; Japanese: japanese.eep; numbered Import/Clear backups |
| Save States | Ten runtime state slots, independent of battery saves |
| ROMs | Your locally imported game image |
| BIOS | Your locally imported GBA BIOS |
| Mods | Your IPS/IPS32/BPS patch and verified patched-ROM cache |
| Captures | F10 diagnostic captures and per-session logs/input recordings |
| Logs | Working directory for other game-generated diagnostics |

No game-owned saves/settings are stored in AppData or the registry. This does
not prevent Windows, graphics drivers or file dialogs keeping their own OS data.
Use a writable folder; there is no silent fallback to another storage location.
Move/copy the whole folder, not only the launcher EXE. Do not run two copies
against the same saves. A distributed candidate starts with empty player-data
folders; it does not discover or copy the developer's test saves.

## First-time setup and translation

Supply your own legally obtained Japanese ROM and GBA BIOS in the launcher.
Selected files are copied locally with their original filenames; their originals
are unchanged. A different file with the same name gets a readable suffix such
as `Game (2).gba`. Identical files under that name are reused without overwriting.
Old numeric ROM copies remain usable: verified ones show the game name and
source/patched-ROM status. Use Change ROM to reselect the original file if you
want its original filename restored; the old importer did not record that name.
The patcher's internal verified-output cache still uses content identities.
Japanese launches without a patch. For English, optionally select and enable the
matching **Hajimari_no_Ishi_v1.1.0.f.bps** in Mods. An already-patched ROM is accepted without
double-patching. Each verified revision runs its own matching compiled engine.
Future translation releases may need a matching executable; incompatible output
is rejected. With the original ROM selected, disabling the patch selects Japanese.
For an already-translated ROM, use Change ROM to select the original instead.
Home shows the effective language and save bank. Japanese uses Saves/japanese.eep
and Save States/japanese.state1 through .state10. English uses
Saves/english-1.1.0f.eep and Save States/english-1.1.0f.state1 through .state10.
Older versioned saves and the retired beta's battery.eep/beta.state files are
not automatically migrated. Keep them backed up. Select the desired
language before Import/Clear. No automatic save conversion or cross-language
save-state reuse is performed. The launcher interface remains in English.
ROMs, BIOS, translation patches and player saves are not included by the packager.

## Controls and Assist Tools

Keyboard and standard SDL gamepad inputs work together. Controls contains separate
keyboard and gamepad lists; Assist bindings live only in Assist Tools. Defaults:
arrows = move; X = A; Z = B; C = L; V = R; Enter = Start; right Shift = Select.
Settings are remembered on Play or normal launcher close.

Assist Tools offers an enable switch, rewind, and fast-forward speed (2–10x).
The in-game menu (Escape) exposes the ten-slot selector, Save state and Load state,
plus rewind and fast-forward actions. Slots 1–9 also use Shift+F1–F9 to save and
F1–F9 to load; slot 10 is accessible in the menu. F10 remains diagnostic capture.
Battery saves are made by the game's own save feature and are not save states.
Rewind history exists only in memory for the current session.

No inventory, money, HP or other gameplay cheats have been implemented here.
Those need separate verified game-state hooks and explicit testing.

## Visual filters

**Game aspect ratio** is in **Graphics > Window & Screen** in the launcher and Esc:
Original GBA (3:2), Widescreen (16:9), and Ultrawide (12:5). Changes made while
paused apply on Resume; the choice persists through Reset Game and launcher
restart. The host image uses 240/284/384 by 160 pixels (284 is a pixel-aligned
16:9 approximation). Guest resolution and camera/collision rules do not change.
Unsupported scenes keep their native framing. The desktop window itself is not
resized when switching; unused space is letterboxed. Window scale and fullscreen
are separate controls. Existing installs without a saved choice keep 12:5.

Launcher and in-game **Graphics > Picture** offer separate controls:

- Scaling: Nearest (original), Linear, Sharp fractional, or Smooth 2x.
- Screen effect: Off, LCD Grid, or CRT, with intensity from 0 to 100%.
- Colour profile: Raw/Unlit/Frontlit/Backlit/Classic, available in both menus
  and independent of filters and aspect ratio.

Nearest with effects Off remains the default. Sharp preserves crisp integer
scaling and softens only the fractional finish. Smooth 2x is an independently
implemented, conservative edge-aware filter, **not xBRZ**. It may soften text and
costs additional processing. Early pre-optimization measurements do not describe
the current candidate. Current-build profiling and weaker-hardware acceptance
are separate release checks; no minimum frame-rate promise is made here.
Use Nearest or Sharp with effects Off when frame rate matters. CRT is a lightweight
scanline/grille/vignette look,
not a full CRT hardware simulation. LCD and CRT are alternatives, not simultaneous
effects. Intensity zero disables their visible effect.

Filters affect only the final game image, not game logic or native launcher/Esc
text. Settings are saved to Settings/launcher.ini and survive reopening and
Reset Game. Unsupported texture paths fall back to the original image and log
the failure. If performance is reduced, choose Nearest or Sharp and effects Off.

## Credits

The Credits page has two independently scrolling panels. Edit
`Credits/original-game.txt` and `Credits/pc-port.txt` as UTF-8 plain text, then
close and reopen the launcher. Missing/empty files show Pending A / Pending B.
Neither editing credits nor opening their page changes saves or gameplay.
The Tools & Open-Source Projects button opens a separate scrollable list with
clickable website links. Edit `Credits/tools-and-projects.txt` to update it;
put each HTTP(S) link on its own line. Links open only when clicked. The list
distinguishes included tools from research references and is not a full license
inventory. Back to Credits returns to the two personal-credit panels.

## Current validation

See [current release verification and open gates](RELEASE_TESTING.md).
The [pre-Guard checkpoint](PRE_GUARD_CHECKPOINT.md) and
[original launcher validation](PORTABLE_LAUNCHER_VALIDATION_20260924.md) remain
historical evidence, not current release certification. Physical controllers,
human visual quality and clean-machine behavior still require acceptance.
This is a beta candidate, not a 1.0 release.
