# Modding and contributor entry points

This is a static-recompilation port, not a recovered, fully documented original
C source tree. Generated functions preserve machine-level behavior; inferred
names and Ghidra pseudocode are research leads, not proof of a function's purpose.
Start with the [verified build workflow](DEVELOPMENT.md) and exact input identities.

## Choose the layer you mean to change

| Change | Durable source to inspect |
| --- | --- |
| Game-owned launcher options and Guard preference | `src/main.cpp`, `src/guard_experiment.h`, `src/guard_preferences.h` |
| Battle framing and Repeat scenery | `src/custom_renderer.cpp`, `src/battle_follow_camera.h`, `src/battle_scenery_repeat.h` |
| Field identity, maps and object residency | `src/custom_field_scene.h`, `src/custom_field_objects.h` |
| Graphics preset recipes | `src/graphics_presets.h` |
| ROM/language/save compatibility | `src/portable_language.h`, `packaging/portable-beta/target.json` |
| Translation-specific generation metadata | `symbols/swordcraft3_release110.toml`, game CMake targets |
| Generic CPU/PPU/audio/input/runtime behavior | pinned `gbarecomp` repository |
| Generic launcher controls and menu framework | pinned `recomp-ui` repository |

Confirm these entry points in the current checkout before editing. Keep game
logic here and reusable engine/UI work in its own repository/branch. Do not
hand-edit generated output as the durable implementation; regeneration must
reproduce your change from reviewed configuration or guarded hooks.

## Native mod versus ROM patch

The PC Guard option and a standalone GBA Guard ROM hack are separate artifacts.
The former hooks this port's runtime; the latter changes instructions in a
specific ROM. See [standalone Guard tools](../tools/guard_romhack/README.md) for
their exact supported inputs and patch order. Do not assume a BPS patch can be
applied before any future translation: source checksums and overlapping changes
must be checked for each revision.

Host widescreen rendering is not simply a portable GBA BPS feature. The PC port
has a wider host buffer, additional map/layer logic and different presentation
resources. A GBA/emulator widescreen hack would require a separate design and
verification; changing only the ROM cannot transplant this host renderer.

The launcher can apply supported patches, but a successful patch application
does not make arbitrary changed code compatible with the static executable.
Require a matching ROM identity/generated corpus. Preserve version-specific
saves and never retag another revision's save-state header as a migration.

## Minimum evidence for a contribution

1. Record game/engine/UI revisions, input hashes, supported scene and intended
   behavior. State which guest-state changes are allowed and which must remain
   identical; a rendering-only claim should not silently alter game rules.
2. Add source/synthetic boundary tests and run the ROM-free suite. Use private,
   same-ROM state/input traces for runtime work; never commit ROMs, BIOS, saves,
   generated guest code or raw game-memory fixtures.
3. Exercise the changed path, a relevant transition, and a negative/fallback
   case. Prove that the feature engaged; an unchanged hash with zero active
   widened frames is not a widescreen test.
4. Report automated, hardware, audio and human-visual coverage separately.
   Respect the repository's state-based investigation rules in [AGENTS.md](../AGENTS.md).
5. Package from a clean committed revision using the current helper. Keep old
   playable folders intact and get approval before promotion/publication.

See [release test coverage](RELEASE_TESTING.md), [tool index](../tools/README.md)
and [historical research](history/README.md). The history is useful context, but
its older translation addresses, launch commands and results are not current
compatibility guarantees.
