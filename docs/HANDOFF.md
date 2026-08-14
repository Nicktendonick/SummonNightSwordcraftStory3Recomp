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
- Experimental Native, Adaptive, and fixed 16:9 views. Reviewed field layers
  use reflected nearest-edge samples because their 256px ring buffers cannot
  supply true off-screen columns. Battles combine authored columns from a
  reviewed 512px arena layer with a reflected foreground layer. Native HUD and
  dialogue chrome stay centered, margin sprites are clipped, and unreviewed
  layouts pillarbox. See the margin-policy revision in
  [VALIDATION.md](VALIDATION.md).
- A deterministic Native/Wide route auditor with exact center comparison,
  per-frame game-policy telemetry, temporal seam/freeze ranking, reusable
  BG/OBJ isolation, strict coverage checks, raw-evidence reuse, and an HTML
  report. Its current canonical route validates only fail-closed pillarboxing;
  overworld and battle route coverage remain the next acceptance milestone.

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

The rebuilt Windows beta target was captured from the field-dialogue and first-
battle save slots at 240x160 and 284x160. Both widened captures preserved the
centered native image exactly and had no black margin pixels. The field used
reflected scenery; the battle used authored BG1 arena columns plus its reflected
BG2 foreground. Adaptive 262x160 and broader gameplay captures should still be
retaken before release. Local screenshots live below the ignored
`validation/adaptive-widescreen` directory and are not part of a normal clone.

## Known limitations and review targets

- Widescreen remains experimental. Free exploration, map transitions, later
  battles, menus, and display-mode effects need broader manual coverage.
- Battle widening combines authored and reflected presentation layers; it does
  not expand simulation or camera geometry. Sprites, collision, and HUD
  coordinates remain native.
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
