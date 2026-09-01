# Developer handoff

Prepared on 2026-08-13 for a new developer reviewing the Summon Night:
Swordcraft Story 3 static-recompilation project.

## Read this first

This is an early, playable engineering project rather than a finished PC port.
It requires a legally obtained Japanese game ROM and GBA BIOS from the person
building it. Do not send or commit ROMs, BIOS files, translation patches, save
files, generated ROM-derived code, or build outputs.

The current work spans three Git repositories. Clone the root recursively so
Git checks out the exact published submodule commits; a root-only clone is
incomplete.

## Repository layout and ownership

| Area | Current branch | Intended contents |
|---|---|---|
| Root game repository | `agent/assist-and-native-pacing` | Swordcraft 3 configuration, game-specific scene policy, beta target, validation, and documentation |
| `gbarecomp` submodule | `feature/swordcraft3-reusable` | Reusable runtime pacing, adaptive-view callbacks, Assist runtime services, save-state/rewind plumbing |
| `recomp-ui` submodule | `feature/gba-assist-bindings` | Reusable launcher/runtime-menu controls and IPS/IPS32/BPS patch support |

The root remote is
`https://github.com/Nicktendonick/SummonNightSwordcraftStory3Recomp.git`.
The development checkouts retain mstan's upstreams as `origin` and use Nickt's
forks as writable remotes. Root `.gitmodules` points at those forks so a fresh
recursive clone can fetch the pinned feature commits.

Keep game-specific work in the root repository. Put generally reusable GBA
runtime work in `gbarecomp`, and generally reusable launcher or patch-format
work in `recomp-ui`.

## Implemented locally

- Static-recompilation scaffold and a 32-shard generated corpus.
- Recompiled GBA BIOS support using a locally supplied BIOS dump.
- Native GBA presentation pacing near 59.7275 FPS, FPS title display, and a
  freely resizable window.
- A fix for the first-cutscene host call-stack overflow.
- Assist Tools with 10 save-state slots, fast-forward, a 2x-10x speed slider,
  one-second rewind, a ten-second rewind history, and configurable keyboard and
  controller bindings.
- Launcher support for IPS, IPS32, and BPS patches without modifying the source
  ROM.
- A separate English-beta executable generated from the locally supplied BPS
  and verified Japanese ROM.
- Experimental Native, Adaptive, fixed 16:9, fixed 2:1, and fixed 384x160
  full-arena views. All seventeen standard battle configurations declare a
  384-pixel logical width. Reviewed
  field layers use authenticated source maps where available. Normal battles
  now sample their finite maps once in natural camera order instead of
  mirroring or repeating them. The complete 512-pixel BG0 raster map was
  checked in the forest and village arenas; BG0 is limited to the
  VCOUNT-selected scanlines 18..123 so its shared HUD rows are not duplicated.
  Reviewed field/battle object culling widens with the view; unreviewed layouts
  pillarbox. The previous repeating/reflected policies and 320-pixel ceiling
  remain available through rollback launchers in the project root. See the
  margin-policy revisions in
  [VALIDATION.md](VALIDATION.md).
- A deterministic Native/Wide route auditor with exact center comparison,
  per-frame game-policy telemetry, temporal seam/freeze ranking, reusable
  BG/OBJ isolation, strict coverage checks, raw-evidence reuse, and an HTML
  report. The stock route validates fail-closed pillarboxing; a separate beta
  recording covers the full first battle and post-battle field/cutscene route,
  but remains diagnostic-only because three dynamic beta gaps are not static.

See [README.md](../README.md), [BRINGUP.md](BRINGUP.md),
[VALIDATION.md](VALIDATION.md), and
[WIDESCREEN_AUDIT.md](WIDESCREEN_AUDIT.md) for the detailed history,
measurements, and audit workflow.

## Private local inputs

The checked-in metadata expects:

| Input | Required identity |
|---|---|
| Japanese ROM | SHA-1 `3f5253fcf57e07ce52472bd29a61d16b98a12376`, CRC32 `12AFAE5D` |
| GBA BIOS | SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492` |
| English beta output | SHA-1 `bb2eebf98deb59bb6218442c2308bb5033ae2915`, CRC32 `A8F22FCA` |

The English beta BPS itself is private/local and intentionally ignored. A new
developer should supply their own legally obtained base ROM and BIOS. Share
the translation patch only under its author's distribution terms.

Expected local locations are described in [baserom.md](../baserom.md). The
pre-handoff audit can be run with:

```powershell
./tools/check_handoff.ps1
```

Use `-FailOnDirty` only after the commits and submodule pins are ready.

## Build and validation entry points

Follow the setup section in [README.md](../README.md). The important project
commands are:

```powershell
git submodule update --init --recursive
./tools/generate.ps1
./tools/generate_bios.ps1
./tools/validate.ps1
./tools/validate_assist_runtime.ps1 -BuildDir build-assist
```

Known local targets are:

- `SummonNightSwordcraftStory3Recomp` for the Japanese ROM corpus.
- `SummonNightSwordcraftStory3RecompBeta` when CMake is configured with
  `SWORDCRAFT3_BETA_BPS` and `SWORDCRAFT3_STOCK_ROM`.

The most recent local build completed both targets on Windows with MSYS2
MinGW GCC 14.2, Ninja, SDL2, and CMake. A stock rebuild may recompile all 32
large generated shards; use one compiler worker on memory-constrained systems.

## Latest widescreen evidence

The rebuilt Windows beta target replayed frames 6,700..17,440 from the first
battle through its post-battle cutscene and field/dialogue route. Aligned
Native/Wide centers matched exactly before battle OBJ culling was expanded.
Frame-by-frame BG isolation exposed and then verified the fixes for empty and
stale margin columns. A later 320x160 pass through the complete first battle
uses full-span periodic continuation for all three normal-arena planes. A
focused replay rejected BG0's nominal 480-pixel allocation because its latter
half is a partial duplicate followed by transparent/corrupt padding; its
complete usable period is 240 pixels. With periods 240/384/128, the focused
seam warning disappeared and the 2,940-frame fight replay reported no blank
margins, temporal freezes, or native-boundary seams. The same route later
passed at the table-declared full 384-pixel arena width. Adaptive intermediate
widths, the village arena, and later-game routes still need owner coverage.
Local evidence lives below the ignored `validation/adaptive-widescreen`
directory.

The 2026-08-27 visible-debugger session added a free-overworld regression at
frames 19,663 and 20,440. It found that the game-owned map descriptor can lag
the live BG hardware scroll by one pixel, which made true-map margin tiles
change one pixel late. The game adapter now aligns the descriptor's full map
page to `BGxHOFS`/`BGxVOFS`. A 30-sample recorded-motion replay preserved every
native-center pixel and produced no strong native-boundary seams.

The next overworld pass found that missing margin objects were not a renderer
placement bug: `sub_08009840` discarded sprites outside X=-63..239 before OAM
was copied. The game config now opts its two horizontal culling immediates at
`0x08009B9E` and `0x08009BB4` into gbarecomp's exact-PC enhancement seam. The
game adapter widens those limits by the active left/right view margins only
when a true-map field scene is identified, and releases the renderer's native
OBJ clip for that scene. Battles, menus, unsupported field layouts, vertical
culling, native-width play, and guest memory outside the normal OAM builder
remain unchanged.

A later owner capture at frame 33,123 placed the player in the same overworld
as an old woman near the right edge. A deterministic replay walked past that
NPC and into the neighboring transition over frames 33,125..34,045. The old
woman remains visible while real true-map scenery exists in the added margin.
Transparent world pixels at the map/transition edge now form a final black
foreground mask above widened OBJ and screen-space portrait layers, so neither
can walk across deliberate black boundaries. The game enables this only for
its validated true-map BG1..BG3 set. The reusable, default-off compositor seam
is `g_ws_margin_occlusion_layers` on gbarecomp's
`feature/swordcraft3-upstream-v2-20260820` branch.

The focused PPU test covers an OBJ and a BG0 portrait-like layer over adjacent
transparent/opaque world pixels, verifies the black boundary wins only over
the transparent pixel, and keeps OBJ-only debugger output raw. The 93-sample
owner-route replay had matching Native/Wide guest state and no capture
integrity failure. Its retained blank-margin findings describe the requested
physical transition boundary; that new route has not been added to the older
release contract's exact-frame allowlist.

## Known limitations and review targets

- Widescreen remains experimental. Free exploration, map transitions, later
  battles, menus, and display-mode effects need broader manual coverage.
- Battle widening loops each plane at its measured/verified usable visual span,
  limits BG0 to the active arena raster band, and widens the reviewed horizontal
  OAM-builder limits for recognized normal arenas. It does not
  expand collision or camera geometry, and HUD/unsupported effect layouts
  remain native. Later arenas still need manual edge testing; the additional
  OAM staging entries also make whole-state Native/Wide hashes differ in IWRAM
  and OAM while the battle policy is active.
- ROM patches that change executable code need a matching static corpus. The
  beta therefore has its own executable instead of using the stock corpus.
- Some English-beta battle-state headless checks still bridge dynamic IWRAM
  execution through the runtime self-heal path. Review the ignored coverage and
  miss reports before claiming fully static beta coverage.
- Audio/pacing is improved, but long fights and multiple host refresh rates need
  listening and frame-time validation on another machine.
- Save states are tied to the ROM identity and snapshot format; they are not a
  stable cross-version interchange format.
- Clean builds are intentionally unable to work without user-supplied private
  inputs and locally regenerated code.

## Change-set boundaries

The root change set owns `.gitignore`, CMake/game configuration, documentation,
the beta target, and `src/adaptive_widescreen.cpp`/`.h`. Run the audit script
for the authoritative current worktree state.

The published submodule branches include:

- `gbarecomp`: runtime option/callback wiring, adaptive view behavior, pacing,
  and runtime bus/Assist integration.
- `recomp-ui`: launcher/runtime Assist controls and the reusable IPS/IPS32/BPS
  implementation, including new tests and tools.

Do not squash these boundaries together. The reusable changes should remain
reviewable independently from the game adapter.

## Publication order

1. Run `./tools/check_handoff.ps1` and review every dirty path.
2. Commit and push the `gbarecomp` reusable changes to its dedicated branch.
3. Create or select a writable `recomp-ui` fork, commit and push its reusable
   changes to the dedicated branch, and update `.gitmodules` if appropriate.
4. Update the root repository's submodule pointers to those published commits.
5. Commit the root game-specific changes and this handoff documentation.
6. Build both targets again and run the relevant validation scripts.
7. Run `./tools/check_handoff.ps1 -FailOnDirty`.
8. Push the root branch, verify that a fresh recursive clone can fetch both
   submodule commits, and grant the reviewer access to every private repository
   involved.
9. Do not upload the ROM, BIOS, BPS, save states, generated code, screenshots,
   caches, or build directories as a shortcut.

## Suggested first review sequence

1. Read the root changes and `src/adaptive_widescreen.cpp` to understand the
   game-specific policy.
2. Review `gbarecomp`'s callback and runtime interfaces for reuse and API shape.
3. Review `recomp-ui`'s patch parser tests and Assist binding/state flow.
4. Reproduce the stock title/new-game trace and Assist validation.
5. Reproduce the two English-beta widescreen save-slot captures locally.
6. Extend deterministic coverage through a complete battle and a free-overworld
   transition before treating widescreen or audio as production-ready.
