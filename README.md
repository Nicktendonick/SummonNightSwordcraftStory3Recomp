# Summon Night: Swordcraft Story 3 PC Port

Work-in-progress PC static-recompilation port of **Summon Night: Craft Sword
Monogatari — Hajimari no Ishi** (Game Boy Advance, Japan), using
[gbarecomp](https://github.com/mstan/gbarecomp) and
[recomp-ui](https://github.com/mstan/recomp-ui).

## Play the portable version

For a playable download, check the
[GitHub Releases page](https://github.com/Nicktendonick/SummonNightSwordcraftStory3Recomp/releases)
for a **Windows portable ZIP attached as a release asset**. GitHub's automatic
**Source code (zip/tar.gz)** downloads are developer source, not the game.
If a release has no portable asset, it does not provide a ready-to-run build.

Extract the entire portable ZIP into a writable folder and start
**Swordcraft Story 3 Beta.exe**, keeping its `Runtime` folder beside it.
You do not need a BAT file or development tools to play a portable build.

Supply your own verified Japanese ROM and GBA BIOS. Japanese does not require
a translation. English requires a translation revision supported by that
specific build; see the README **inside your downloaded ZIP**. No ROM, BIOS,
translation patch, or personal save is supplied by this repository.

## Current source and release status

The source supports original Japanese and verified **English 1.1.0f** builds.
English revisions have separate generated instruction corpora and save paths;
a future translation is not automatically compatible. See [input identities](baserom.md).

The latest merged scenery work extends **Repeat scenery** to all 17 standard
battle arena descriptors (IDs 0–16), with arena-specific continuations and
native fallbacks for unsupported states. See
[implementation, verification and known limits](docs/REPEAT_SCENERY_20261009.md).
This is not certification of every battle, spell, story route, or visual join.

Other existing features include themed launcher/Esc menus, optional
**Hold Select to Guard**, graphics presets and filters, switchable aspect
ratios, save-state/rewind tools, and F10 diagnostic capture. These remain beta
features; options and compatibility depend on the installed build.

**Release-readiness work is in progress.** Merging source does not update an
already-installed portable. The current packaging recipe and its player README
still describe **1.0.6f**; they must be updated and the resulting candidate tested
before publishing a new 1.1.0f portable. Do not relabel an old ZIP as a new build.

## Build or contribute

Clone the source with its pinned dependencies:

```sh
git clone --recurse-submodules https://github.com/Nicktendonick/SummonNightSwordcraftStory3Recomp.git
cd SummonNightSwordcraftStory3Recomp
```

For an existing clone, run `git submodule update --init --recursive`.
Both dependency repositories are accessible without the former private-repo
invitation requirement; a fresh recursive clone was checked on October 16, 2026.

- [Developer workflow and current build verification](docs/DEVELOPMENT.md)
- [Developer tool index](tools/README.md)
- [Renderer design and investigation handoff](docs/HANDOFF.md) — historical
  observations must be checked against current code.
- [Portable packaging recipe](tools/package_portable_release.py) — pending the
  current-release promotion described above.
- [Distribution review requirements](packaging/portable-beta/RELEASE-REVIEW.md)

Game-specific code belongs here; generic engine changes belong in `gbarecomp`,
and generic launcher/UI changes in `recomp-ui`. Keep ROMs, BIOS, patches, saves,
captures, generated game code, and compiled outputs out of commits.

## Report a problem

Include the portable/revision name, language and translation version, Windows
version, graphics settings, input device, and exact reproduction steps. For a
rendering issue, press **F10** in the running portable and note the capture folder.
Review diagnostics before sharing: do not upload ROMs, BIOS, saves, or raw
game-memory captures publicly. Ask which minimal diagnostic files are needed.

## Historical experiments

Old lake/forest tests, rollback launchers, English-beta wrappers, machine-specific
Ghidra shortcuts, and the alpha-packaging shortcut are preserved as **reference
text**, not current launch instructions. See [history index](docs/history/README.md).
The cleanup does not remove the lake renderer, its tests, useful diagnostic
tools, or notices still required by packaging.
