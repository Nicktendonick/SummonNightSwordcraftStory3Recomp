# Summon Night: Swordcraft Story 3 Recomp

Early PC static-recompilation scaffold for **Summon Night: Craft Sword
Monogatari - Hajimari no Ishi** (Game Boy Advance, Japan), built with
[`gbarecomp`](https://github.com/mstan/gbarecomp) and the
[`recomp-ui`](https://github.com/mstan/recomp-ui) launcher.

For another developer taking over or reviewing the work, start with
[docs/HANDOFF.md](docs/HANDOFF.md).

## Status

The repository wiring, first static corpus, private BIOS recompilation, and
initial headless boot validation are complete. The pinned ROM currently emits
32 local generated shards containing 48,708 discovered entry/resume points.
The Windows debug executable builds and links with MinGW, SDL2, gbarecomp, and
recomp-ui.

A deterministic 4,400-frame trace starts a new game and selects the male
protagonist. A saved-state continuation now passes another 10,000 strict-static
frames through partner selection (guest frame 16,151) with zero dispatch misses
and zero interpreted instructions. This proves static coverage for those
bounded traces only; it does not yet make the project a playable port. A
game-specific route through partner selection, exploration, and combat remains
bring-up work.

## Setup

1. Initialize dependencies: `git submodule update --init --recursive`.
2. Place the verified ROM at `roms/swordcraft3_jp.gba`; see `baserom.md`.
3. Provide a verified GBA BIOS at `gbarecomp/bios/gba_bios.bin`.
4. Build `gba_recompile` in the framework submodule.
5. Run `tools/generate.ps1`.
6. Run `tools/generate_bios.ps1`; its private output is written below the local
   build directory rather than the framework source tree.
7. Configure and build this repository with CMake, Ninja, a MinGW compiler,
   and SDL2 available. Development builds stage the required MinGW/SDL2 DLLs
   and recomp-ui assets beside the executable, so the build directory is
   directly launchable. Generated game code remains optimized in Debug builds
   to avoid the severe unoptimized guest-code slowdown; use a Release or
   RelWithDebInfo build for performance validation.
8. Run `tools/validate.ps1` to replay the strict-static new-game regression.
9. Run `tools/validate_assist_runtime.ps1 -BuildDir build-assist` to measure
   fast-forward pacing and exercise isolated save-state/rewind actions.

The verified local inputs used during bring-up are documented in `baserom.md`.
Neither input nor ROM/BIOS-derived generated code is committed.

## Desktop controls and Assist Tools

The game window is freely resizable. The native 3:2 picture remains
aspect-correct, with letterboxing or pillarboxing when the window uses a
different ratio. The title bar displays the measured FPS by default; it can
also be toggled from the in-game Display section. Normal-speed play targets
the GBA's native 59.7275 FPS, so a healthy counter reads about 59.7 rather
than exactly 60. Renderer VSync is disabled by default so it cannot compete
with that native frame clock.

Press Escape during play to open the recomp-ui runtime menu. Its **Assist
Tools** section has a master enable switch, 10 save-state slots, Save and Load
actions, a persistent fast-forward switch, and a one-second rewind action.
The pre-boot launcher also has an **Assist Tools** page, and the Controller
configuration page repeats its global Rewind and Fast-forward binding chips.
Select a keyboard or controller chip and press the replacement key, button, or
trigger. The defaults match the DKC2 project: `1`/left trigger for Rewind and
`2`/right trigger for Fast-forward. **Reset Assist Controls** restores them.
The Fast-forward speed slider ranges from 2x to 10x and defaults to 4x. It is
available in both the pre-boot Assist Tools page and the in-game Assist Tools
section; changes made in-game apply immediately for the current session.
Rewind keeps the most recent 10 seconds in memory and is cleared when a state
file is loaded. Slot 10 is menu-only; the existing function-key shortcuts
remain slots 1 through 9. While Assist Tools is enabled, hold Tab to
fast-forward as a legacy shortcut, use Shift+F1 through Shift+F9 to save, and
F1 through F9 to load.

Save states are convenience snapshots rather than replacements for normal
in-game saves and are tied to the current ROM and snapshot format.

## Experimental adaptive widescreen

Open **Display** in the pre-boot launcher and cycle **View mode** between
**Native**, **16:9**, and **Adaptive**. Native remains the default faithful
240x160 view. Fixed 16:9 renders a 284x160 game surface. Adaptive follows the
window's live aspect from native 3:2 up to the same 16:9 limit; wider windows
retain clean side bars rather than exposing unvalidated map data.

The game adapter recognizes reviewed Mode 0 field/cutscene and battle layouts.
Field scenes extend into the added area with a reflected nearest-edge
treatment: their 256-pixel ring buffers hold at most 16 valid columns beyond
the 240-pixel viewport, so keeping the wrapped entries showed the opposite
map seam and stale streamer columns as repeated or garbled margin tiles.
Real field continuation is planned via a map-data sidecar that reads the
guest's own map layout. Battle BG1 and BG2 also use reflected nearest-edge
scenery: route-scale layer isolation proved that the nominally 512-pixel BG1
still exposes undrawn columns as the arena camera moves. Native HUD and
dialogue chrome remain centered, and sprites the game parks just off the native
edge are clipped so they never surface in the margins. Affine, bitmap,
forced-blank, menu, and otherwise unsupported layouts fall back to clean
pillarboxing rather than repeating unrelated tiles. The original center image
remains pixel-identical in every mode.

The deterministic title/new-game route has been checked at frames 1,200,
1,800, 3,000, and 4,400 with no static-dispatch misses. A recorded English-beta
route now covers a complete first battle, its display-mode effects, the
post-battle cutscene, and later field/dialogue scenes in aligned Native and
16:9 runs. That route still crosses three dynamic beta coverage gaps, so it is
diagnostic evidence rather than release acceptance. Later battles and broader
exploration still need review; the launcher keeps this feature **Experimental**.

The repository now includes a deterministic route-scale auditor rather than
relying only on hand-picked screenshots. It replays the same input in Native
and Wide modes, verifies the entire 240x160 center pixel-for-pixel, records the
scene policy selected on every sampled frame, and ranks persistent seams,
pillarbox leaks, blank/frozen authored margins, and optional OBJ-isolation
leaks. Raw captures remain ignored and can be reanalyzed without replaying the
game. See [docs/WIDESCREEN_AUDIT.md](docs/WIDESCREEN_AUDIT.md).

## ROM patches and translations

Open **Mods** in the pre-boot launcher to select and enable an IPS, IPS32, or
BPS patch. The launcher always starts from the verified Japanese ROM. It
checksum-validates BPS inputs and outputs, writes the result to
`build-*/mods/rom-patches`, and launches that cached copy. The source ROM and
patch file are never changed.

Patch selection is remembered in `config.ini`, while `rom.cfg` continues to
remember the original Japanese ROM rather than the generated copy. Disabling
or clearing the patch returns to the stock game. User-supplied patches and
patched ROMs are ignored by Git and must not be committed or distributed from
this repository.

Static recompilation adds one important compatibility boundary: text, graphics,
and other data-only changes can use the stock generated code, but a patch that
changes executable ARM/Thumb instructions may need its own generated static
corpus. Validate each translation release before treating it as supported.
The supplied Swordcraft Story 3 beta BPS identifies the verified Japanese ROM
(`CRC32 12AFAE5D`) as its source; its translated output is checked during the
validation workflow documented in `docs/VALIDATION.md`.
That beta changes executable Thumb code, so use its separate private build
target rather than the stock executable:

```powershell
.\.tooling\cmake\data\bin\cmake.exe -S . -B build-beta `
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DSWORDCRAFT3_BETA_BPS="C:/path/to/Summon_Night_Swordcraft_Story_3_Beta.bps"
.\.tooling\cmake\data\bin\cmake.exe --build build-beta `
  --target SummonNightSwordcraftStory3RecompBeta
```

The build applies the local patch, verifies its BPS checksums, generates code
under `build-beta/generated-beta`, and emits a beta-specific executable whose
launcher accepts only the matching translated target. None of those private,
ROM-derived outputs are committed.

The automated startup pacing sample observed exact 1, 2, 4, and approximately
10 guest frames per presentation for the corresponding modes. On the validation
machine, the short startup sample measured 13.61 guest FPS normally and 147.74
guest FPS at the 10x setting (about 2.47x real GBA speed). These are
workload- and machine-specific measurements, not a whole-game benchmark.

## Repository boundaries

All durable project source, builds, configuration, notes, and test artifacts
must remain under
`Documents\Codex\Projects\SummonNightSwordcraftStory3Recomp`.

- Game identity, configs, imported symbol metadata, and future game hooks live
  in this repository.
- Reusable ARM7TDMI, GBA hardware, runtime, and launcher integration changes
  belong in the `gbarecomp` submodule's `feature/swordcraft3-reusable` branch.
- ROMs, BIOS images, saves, and generated ROM-derived C++ are ignored.

See `docs/BRINGUP.md` for the validation result and next milestones, and
`docs/REFERENCES.md` for the research projects used as references.
