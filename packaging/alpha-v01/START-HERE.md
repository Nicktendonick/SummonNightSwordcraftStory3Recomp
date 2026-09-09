# Alpha unfinished-build v.01

Summon Night: Swordcraft Story 3 PC port -- Windows x64, English-beta variant.

**LOCAL REVIEW COPY. Not approved for uploading or redistribution.** Read
`LICENSE-REVIEW.md`. This directory is a packaging experiment, not a published
release or a claim that the game is complete. No ROM, BIOS file, translation
patch, save or captured game artwork is provided.

## Try this copy locally

1. Extract the entire archive to a new writable folder (not Program Files).
2. Double-click `Launch.cmd`. Keep the `assets` folder and all four DLLs next
   to the executable. You do not need Python or the packaging tools to start it.
3. Select your own verified Japanese ROM and GBA BIOS in the launcher. Under
   Mods, select and enable your own compatible English-beta translation patch.
   This executable requires the exact translated ROM identity below; it is not
   the unpatched Japanese executable and does not accept arbitrary future patches.
4. Start in Native display mode. Escape opens the runtime menu; Display offers
   experimental widescreen modes. Assist Tools contains save states, rewind and
   the fast-forward speed slider. See `RELEASE-NOTES.md` for limitations.

| Input | SHA-1 |
| --- | --- |
| Japanese source ROM | `3f5253fcf57e07ce52472bd29a61d16b98a12376` |
| BIOS | `300c20df6731a33952ded8c436f7f186d25d3492` |
| Required translated output ROM | `bb2eebf98deb59bb6218442c2308bb5033ae2915` |

The launcher stores `config-beta.ini`, `keybinds.ini`, `rom-beta.cfg` and
`bios.cfg` beside the executable. Patch output is created under
`mods/rom-patches` there. Normal saves default to a `.sav` beside the ROM
actually used (the patched cached ROM for the Mods flow); save-state slot files
also use the running ROM's path. Keep this directory writable and back up
normal saves before changing builds. An externally selected already-patched
ROM can therefore keep its save outside this folder. Do not share these
generated settings/caches/saves: they may contain private paths or game data.

This package has no compiler or existing self-healing cache. Startup checks
cannot establish that later dynamic-code routes behave the same on a machine
without development tools. Complete `SMOKE-TEST.md` before treating it as a
portable playable build. Save states are not promised compatible across builds.

`MANIFEST.json` identifies every packaged input, its source hash and resulting
hash. `SHA256SUMS.txt` covers package files except itself; the archive checksum
is supplied alongside the ZIP, outside it. Inspect the original archive when
verifying: playing intentionally adds local state files.
