# Astra continuation checkpoint

Prepared 2026-09-07 as the capstone for the first Swordcraft Story 3 PC-port
development phase. The project is playable, but it remains an experimental
static-recompilation project rather than a finished release.

## Published checkpoint

Clone the root repository recursively. A root-only clone is incomplete because
the project intentionally carries reusable work in two submodules.

| Repository | Branch | Published commit |
|---|---|---|
| Game project | `agent/assist-and-native-pacing` | `e2c985a` — finite-map battle widescreen |
| `gbarecomp` fork | `feature/swordcraft3-upstream-v2-20260820` | `c973647` — VRAM frame snapshots and Assist binding exposure |
| `recomp-ui` fork | `feature/swordcraft3-upstream-20260823` | `92fc0ae` — View-mode row and deep-page navigation fixes |

The intended checkpoint tag is `capstone-pre-astra-2026-09-07`. Never commit
the ROM, BIOS, BPS patch, save states, generated ROM-derived code, captures, or
executables. All durable project files belong under
`C:\Users\Nickt\Documents\Codex\Projects\SummonNightSwordcraftStory3Recomp`.

## What works

- Separate Japanese and English-beta builds; the beta uses its own generated
  corpus because the translation modifies executable code.
- Native pacing near the GBA's 59.7275 FPS, an FPS title counter, resizable
  window, and pre-boot/in-game Display controls.
- IPS, IPS32, and BPS patch selection without modifying the source ROM.
- Assist Tools with ten save-state slots, configurable rewind and fast-forward,
  a ten-second rewind history, and a 2x–10x fast-forward slider.
- Experimental field widescreen sourced from authenticated full map data, with
  black transition boundaries composited above sprites and portraits.
- Experimental battle widths through 384x160. Normal battle layers use their
  original camera projection and finite map coordinates rather than mirrored or
  periodically repeated native pixels. Reviewed battle sprites can enter the
  expanded view, while the original 240x160 center remains unchanged.
- One-click rollback launchers for the prior 320-pixel ceiling, repeating
  battle margins, and reflected battle margins.
- Deterministic Native/Wide route auditing, per-layer frame capture, policy
  telemetry, exact native-center comparison, and seam/freeze diagnostics.

## Last verified build

Local English-beta executable:

`build-beta\SummonNightSwordcraftStory3RecompBeta.exe`

SHA-256:

`0BAFB1FC1E24EA41670DFFB423E8274C1A8A25438787710AD6521641C176A190`

The final Slot 5 village replay sampled guest frames 11,431–11,435 at 384x160.
It selected `battle_natural`, preserved identical native-center pixels and
Native/Wide guest state, and produced zero encoded seam or margin findings.
Evidence is local and intentionally ignored at:

`validation\adaptive-widescreen\battle-state5-natural-final-384-20260831`

The forest frame-4,980 state was also inspected. Its right side continues from
the real BG0/BG1 maps; its sloped black area at the far left is a finite arena
boundary, not a mirror.

The focused test set passed:

```powershell
$env:PATH = 'C:\msys64\mingw64\bin;' + $env:PATH
.\.tooling\cmake\data\bin\ctest.exe --test-dir build-beta -R '^(swordcraft3_widescreen_route_audit_unit|runtime_monolith_guard|ppu_smoke_tests)$' --output-on-failure
```

The widescreen audit has 13 unit tests. English-beta battle-state captures are
diagnostic rather than release-eligible because copied IWRAM code still needs
interpreter/self-heal coverage in those routes.

## Architecture boundary

- Keep game signatures, arena rules, palette choices, scene-specific layout,
  and Swordcraft memory interpretation in the root game repository.
- Put only reusable GBA runtime capabilities in `gbarecomp`, on its existing
  feature branch.
- Put only reusable launcher/runtime-menu behavior in `recomp-ui`, on its
  existing feature branch.
- Publish submodule commits before updating and publishing the root pointers.

The main widescreen adapter is `src/adaptive_widescreen.cpp`. Shared renderer
hooks are in `gbarecomp/src/gba/gba_ppu.cpp` and the frame-data bridge is in
`gbarecomp/src/runtime/runtime.cpp`/`runtime.h`.

## Important remaining risks

1. Later arenas, bosses, menus, affine/bitmap effects, and unusual battle
   transitions still need owner recordings and deterministic captures.
2. The translation's dynamic-IWRAM execution gaps prevent a fully static claim
   for the English-beta battle routes.
3. Audio and pacing need another long-fight listening/frame-time pass on the
   owner's machine, including fast-forward and rewind recovery.
4. Adaptive resizing needs broader live-resize testing at intermediate widths.
5. There is no polished redistributable package or release workflow yet.
6. Save states are tied to ROM identity and snapshot format and should not be
   advertised as stable across versions.

## Recommended next task

Implement the owner's battle-layout mock-up as an optional game-specific
presentation setting:

- keep gameplay scenery expanded only through the battle raster band
  (scanlines 18–123);
- leave the original top and bottom HUD artwork centered and pixel-identical;
- fill only the added left/right columns above and below that band with the
  active pale-tan HUD/background color;
- optionally continue the thin brown separator line, without stretching bars,
  text, icons, or gauges;
- derive colors from live game palette/frame data where practical so fades and
  palette effects remain coherent;
- apply the treatment only to positively identified normal battles and fail
  closed for menus/effects;
- expose a reversible `Battle HUD borders` option after the visual rule is
  proven, keeping the setting game-specific unless a genuinely reusable UI
  primitive is required.

Validate Native, 16:9, 2:1, 384x160, and live Adaptive resizing. Reuse Slot 5
and the forest frame-4,980 capture, isolate BG0/BG1/OBJ, and require the complete
240x160 center to remain byte-identical. Do not push presentation experiments
until the owner accepts screenshots or a playable build.

After that, prioritize later-arena coverage, then the remaining dynamic-IWRAM
static corpus, then long-session audio/performance and release packaging.
