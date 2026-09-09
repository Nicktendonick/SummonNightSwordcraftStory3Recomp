# Astra continuation checkpoint

## Latest source checkpoint (2026-09-09)

The reviewed forest critical-hit Mode 1 layout now keeps wide scenery and tan
HUD wings without looping the affine impact canvas. See
[CRITICAL_HIT_WIDESCREEN.md](CRITICAL_HIT_WIDESCREEN.md) for tested build hashes,
same-width before/after/rollback results, scope limits, and the rollback launcher.
`Launch Debug Capture.bat` now opens the normal settings launcher before Play.
Camera limits and translucent edge overlays were discussed but not implemented.

The owner authorized pushing this source checkpoint after being informed that
the game repository is public. The gbarecomp dependency remains private; its
reusable scanline-context changes are commit `f85f0bc` on
`fix/widescreen-scanline-margins-20260909`. A public clone still requires access
to private submodules. No repository visibility change or binary release is
part of this source push. Earlier notes below describe their historical state.

The subsequent session `20260909-150403-385-beta` had recording/performance
logs but no exported F10 snapshots when inspected. It is not additional visual
validation; the critical-hit snapshot tests remain the evidence for this fix.

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

## Owner's prerelease requirement (2026-09-07)

After implementation and owner testing, prepare the GitHub prerelease workflow
described in [PRERELEASE_PLAN.md](PRERELEASE_PLAN.md). The capstone tag is a
development checkpoint, not a release. Package contents and distribution
provenance must be reviewed before any executable is selected for staging.
Publishing a release or changing repository visibility requires an explicit
owner request after review of the final artifact list.

The owner chose the title **Alpha unfinished-build v.01** and requested local
release preparation. `Prepare Alpha Release.bat` invokes the game-specific
allowlist packager in `tools/package_alpha.py`. It pins the existing English-beta
executable hash, strips debug data from COPIES while checking loaded sections,
includes required runtime DLLs and available notices, and produces inventory,
checksums and restricted-PATH startup checks under the ignored `release-stage`.
It never publishes. See `packaging/alpha-v01/LICENSE-REVIEW.md` for unresolved
compiled game/BIOS distribution, license/provenance and clean-machine gates.
The proposed tan HUD borders are not part of this release snapshot.

## Local continuation: battle HUD borders (2026-09-08)

The tan mock-up is now implemented in the local development executables,
separately from the unchanged Alpha archive. See
[BATTLE_HUD_BORDERS.md](BATTLE_HUD_BORDERS.md) for exact build hashes, before/after
captures, validation coverage and rollback instructions. The in-game path is
Escape → Display → Battle HUD borders (default on; not yet a pre-boot row).
All new code is in the game repository; the existing shared hooks were enough.

Village/forest five-frame replays passed at 284/320/384; Native centers and the
gameplay band are protected, and Off matches the prior capstone's complete
pixels/guest state. The verified overworld/NPC replay is unchanged. The forest
has an existing Native/Wide guest-state discrepancy, reproduced unchanged in
the old executable; retain it as a separate audit issue rather than claiming
the border test resolved it. Four focused CTest checks passed.

Next: owner visual acceptance, actual runtime-menu toggling, live resizing,
moving battle cameras and attack/pause/transition effects. Do not push this
presentation experiment until the owner accepts the screenshots/playable build.

## Local correction (2026-09-09)

The September 8 user captures exposed gauge colors extruded into tan margins,
an unsupported taller START-paused HUD, and an off-by-one BG0 margin crop.
Those are corrected in both `build-beta` executables; see the latest section
of [BATTLE_HUD_BORDERS.md](BATTLE_HUD_BORDERS.md) for hashes, reproduction and
rollback details. The earlier exact capstone-Off claim above is historical:
Off now retains the corrected BG0 margin rows 18/124. Guest state and native
center remain protected. Everything is game-owned; submodules remain clean.

Next unresolved issue: particle popping at arena edges, with a specific but
unproven signed-OAM-X lead documented there. Larger finite-map left/right
coverage differences also remain open. Do not claim those fixed by the HUD
and corner seam work. The Alpha archive remains unchanged; nothing pushed.

## Subsequent battle-layer continuation (2026-09-09)

The new F10 capture proved that part of the attack is on BG2, whose 256-pixel
wrap caused duplicate edge flashes. The owner then explicitly requested fully
matching, looping distant scenery, supplying a mountain-backdrop example.
See [BATTLE_LAYER_CONTINUATION.md](BATTLE_LAYER_CONTINUATION.md) for the current
implementation, exact-loop tests, rollback launcher, evidence, and limitations.
The forest backdrop uses a verified 160-pixel cycle; effects use one copy.
The game now uses an opt-in actual-scanline context hook in gbarecomp, alongside
a fix for debugger layers leaking into authored margins. Those reusable edits
are isolated on `fix/widescreen-scanline-margins-20260909`.
Overworld boundaries and authentic native-center rendering remain unchanged.
The earlier silhouette-clipped backdrop experiment was superseded by the
owner's full-loop clarification. No release archive or remote was changed.
