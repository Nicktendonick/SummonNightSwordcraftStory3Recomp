# Summon Night: Swordcraft Story 3 Recomp

Early PC static-recompilation scaffold for **Summon Night: Craft Sword
Monogatari - Hajimari no Ishi** (Game Boy Advance, Japan), built with
[`gbarecomp`](https://github.com/mstan/gbarecomp) and the
[`recomp-ui`](https://github.com/mstan/recomp-ui) launcher.

## Status

The repository wiring, first static corpus, private BIOS recompilation, and
initial headless boot validation are complete. The pinned ROM currently emits
32 local generated shards containing 48,708 discovered entry/resume points.
The Windows debug executable builds and links with MinGW, SDL2, gbarecomp, and
recomp-ui.

A deterministic 4,400-frame trace starts a new game and selects the male
protagonist. It passes gbarecomp's strict-static gate with zero dispatch misses
and zero interpreted instructions. This proves static coverage for that bounded
trace only; it does not yet make the project a playable port. Longer scripted
traces, rendering/audio comparison, save persistence, and full-game coverage
remain bring-up work.

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

The verified local inputs used during bring-up are documented in `baserom.md`.
Neither input nor ROM/BIOS-derived generated code is committed.

## Desktop controls and Assist Tools

The game window is freely resizable. The native 3:2 picture remains
aspect-correct, with letterboxing or pillarboxing when the window uses a
different ratio. The title bar displays the measured FPS by default; it can
also be toggled from the in-game Display section.

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
