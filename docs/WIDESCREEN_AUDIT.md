# Adaptive-widescreen route audit

The Swordcraft 3 widescreen workflow now uses deterministic route-scale
evidence in addition to individual screenshots. This directly addresses the
failure mode that previously escaped checkpoint review: a 256-pixel rolling
tilemap can preserve the native center and contain non-black margin pixels
while those margin cells are still stale or from the opposite wrapped page.

## Transferable lessons

The DKC2 audit work established several rules that apply here:

1. Compare aligned Native and Wide runs. Every native-center pixel must remain
   identical; a non-black margin is not sufficient evidence.
2. Record the selected source/policy, not just the final image. Swordcraft logs
   whether each sampled frame used field reflection, authored battle
   continuation, or fail-closed pillarboxing, plus BG registers and scroll.
3. Audit time, not only isolated frames. Persistent boundary seams and margins
   that remain frozen while their authorized BG scrolls are better candidates
   than a single unusual column.
4. Isolate the failing stage. The shared renderer can capture BG0..BG3 or OBJ
   independently through a debug-only layer mask; normal rendering defaults to
   the unchanged full composite.
5. Preserve raw evidence. `--reuse-capture` retunes detectors and regenerates
   JSON/HTML without rerunning the game or replacing the original images.
6. Fail closed. Unclassified, affine, bitmap, forced-blank, menu, and special
   layouts remain centered until a specific scene/layer source is proven.
7. A clean automatic report is not a correctness proof. Artistic intent,
   priority, object activation/culling, and true field-world identity still
   need owner review or a reference/map-data oracle.
8. Deterministic guest input is not enough if live host controls remain active.
   Audit runs make the replay authoritative and disable SDL controller
   backends, preventing a connected controller from rewinding only one side of
   a Native/Wide comparison.

## Coarse canonical audit

Use a Release or RelWithDebInfo executable. Python 3 is required. The command
below samples the existing deterministic new-game route every 12 guest frames:

```powershell
.\tools\run_widescreen_audit.ps1
```

The wrapper finds the project's available Python runtime, uses dummy audio and
video drivers for unattended capture, and includes the most useful BG/OBJ
isolation passes. Give it a different `-OutputDir` for each preserved run. The
equivalent lower-level command is:

```powershell
python .\tools\audit_widescreen_route.py `
  --executable .\build-assist\SummonNightSwordcraftStory3Recomp.exe `
  --rom .\roms\swordcraft3_jp.gba `
  --bios .\gbarecomp\bios\gba_bios.bin `
  --config .\game.toml `
  --input-replay .\tests\input\new_game_select_male.trace `
  --output-dir .\validation\adaptive-widescreen\route-audit-new-game `
  --start 1200 --end 4400 --step 12
```

The default captures the Native and Wide composites. Add isolated layers only
when they answer a concrete question, because every requested layer replays the
route again:

```powershell
  --layers composite,bg1,bg2,obj
```

Run a coarse pass first. For a candidate near frame 3,600, capture every frame
in a narrow interval:

```powershell
python .\tools\audit_widescreen_route.py <same inputs> `
  --output-dir .\validation\adaptive-widescreen\route-audit-3590-3620 `
  --start 3590 --end 3620 --step 1 --layers composite,bg1,bg2,obj
```

To regenerate only the derived report:

```powershell
python .\tools\audit_widescreen_route.py <same arguments> --reuse-capture
```

## Report contents

`report.json` is the machine-readable interface. `index.html` links the aligned
Native, Wide, and available isolated-layer evidence. Private ROM, BIOS, saves,
states, input recordings, screenshots, and raw logs remain below the ignored
output directory and must not be committed.

Current detector classes are:

| Finding | Meaning |
| --- | --- |
| `capture_integrity` | an expected image or aligned policy record is absent or malformed |
| `native_center_mismatch` | the authentic 240x160 center differs between Native and Wide |
| `pillarbox_policy_leak` | a fail-closed scene emitted non-black margin pixels |
| `authored_margin_blank` | a claimed field/battle continuation is entirely black beside a nonempty center |
| `native_boundary_seam` | a persistent discontinuity sits at the former native edge |
| `margin_temporal_freeze` | authorized BG scroll and center motion continue while both margins remain nearly static |
| `obj_native_clip_leak` | an OBJ-only capture contains margin pixels despite the native-clip policy |

`unclassified_scene_pillarboxed` is a safe observation, kept separately so
future coverage work can prioritize common unsupported scene signatures.

## Remaining oracle gap

The current field policy deliberately reflects nearest-edge scenery because
the live 256-pixel ring cannot provide a whole 284-pixel world view. The audit
can prove center preservation and detect several temporal/presentation defects,
but it cannot prove true field continuation. That requires the planned csm3
map-data sidecar: resolve the decompressed map-index source, identify a stable
world coordinate for each margin cell, and compare the same tile identity when
it later enters the native view. Until then, reflected field margins remain a
safe presentation fallback rather than authentic expanded map geometry.
