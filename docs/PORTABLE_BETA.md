# Portable beta launcher

Open **Swordcraft Story 3 Beta.exe**. Keep the entire folder together.

| Folder beside the launcher | Contents |
| --- | --- |
| Runtime | Game executable, required DLLs and launcher artwork/fonts |
| Credits | Editable original-game and PC-port credit text files |
| Settings | Launcher controls, display preferences, relative file selections and cover rotation |
| Saves | In-game battery save: battery.eep; numbered backups from Import/Clear |
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
against the same saves. The developer launcher copies existing test saves once,
without deleting the originals or replacing already-existing portable saves.

## First-time setup and translation

Supply your own legally obtained Japanese ROM and GBA BIOS in the launcher.
Selected files are copied locally; their originals are unchanged. In Mods,
select the matching English-beta BPS (IPS/IPS32 is also supported by the patcher).
An already-patched ROM matching this executable is accepted without double-patching.
This build requires the exact beta output it was recompiled for. Future translation
releases may need a matching executable; incompatible output is rejected.
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

See [the validation report](PORTABLE_LAUNCHER_VALIDATION_20260924.md)
for actual build/test results. Controller hardware behavior
and the final UI layout still need player acceptance; synthetic tests alone do not
certify them. This is a functional beta, not a 1.0 release.
