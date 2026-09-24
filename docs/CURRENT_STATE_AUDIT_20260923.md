# Current-state audit — 2026-09-23

## Conclusion

The owner-approved full-frame combat checkpoint exists, matches its recorded
executable, and has been checkpointed on a separate feature branch. The field
renderer already widens compatible maps, draws supported NPCs/shadows, and
handles regular field animations, but still preserves the stock 240x160 center.
Migrating field composition to full-frame ownership is the remaining architectural
step; rebuilding field widening or its already-fixed controls from scratch is not.

The ordinary launchers in the owning project folder still select the preceding
hybrid build. The unfinished upstream engine/UI integration is a third, separate
checkout and is not part of either playable executable. Old status notes must not
be read as a single current backlog.

This audit inspected source, Git state, launcher routing, executable hashes,
stored test identities/results and project records. It did not run a game, rebuild,
change runtime behavior, merge dependencies, modify saves, or publish anything.
Only this report was added. No screenshots or framebuffer comparisons were used
as assertions. Remote-tracking references were inspected locally; no fresh
upstream/network update check was performed.

## 1. Which version is which?

All paths below are relative to the owning Documents project:
`C:/Users/Nickt/Documents/Codex/Projects/SummonNightSwordcraftStory3Recomp`.

| Checkout | Game HEAD / branch | Current role |
| --- | --- | --- |
| Project root | `210aa415` / `agent/assist-and-native-pacing` | Older adapter plus uncommitted camera/map experiments and wrappers. Not the new renderer source. Preserve its unrelated work. |
| `experiments/custom-renderer` | `1af07ad2` / `experiment/custom-combat-renderer-20260913` | Accepted hybrid field/combat baseline, including the September 22–23 fixes. Rollback build. |
| `experiments/full-viewport-renderer` | `a3a082f3` / `experiment/full-viewport-renderer-20260923` | Owner-approved complete-frame combat experiment; field remains hybrid. Audit target. |
| `experiments/upstream-integration` | `1af07ad2` / `integration/upstream-20260923` | In-progress dependency merges, not a promoted or verified game build. |

The full-viewport game HEAD matches its local `origin` tracking reference.
Its pinned dependencies are:

- gbarecomp `f691f802d9078f6a23b76f4ce25075f7584de288`, branch
  `experiment/full-viewport-primitives-20260923`, clean and tracking `private`.
- recomp-ui `92fc0aea585a933e92db17d5079d6d09b6600613`, clean detached checkout.

The game checkout reports `native-test.toml` modified, but its normalized Git
object and HEAD blob are both `f6a99728ddee92ddae876aa0f300e6d4e8410ddb`,
with no content diff. This is not evidence of an uncommitted settings change;
the file/index was left untouched. Root dirty files and integration merge state
were also left untouched.

### Executable identities verified during this audit

| Build | SHA-256 |
| --- | --- |
| Full-viewport `build-native/Swordcraft3CustomRendererBeta.exe` | `c29226f597d3a5fb0e4480f0db4cc43dc3ec348c676b6c9bcf5e4bbc0024215b` |
| Accepted custom-renderer executable at the equivalent path | `ef8ca3cb9dec56166abe85b852af3f7a97ee87a5151a4ea38a1dddae6847538e` |

### Launcher routing matters

| Launcher | Actual destination / behavior |
| --- | --- |
| Root `Launch General Field 12x5 Test.bat` | Calls `experiments/custom-renderer`; not the new full-frame combat build. |
| Root `Launch Custom Renderer - Overworld and Combat.bat` | Also calls `experiments/custom-renderer`. |
| `experiments/full-viewport-renderer/Launch Full Frame Combat 12x5 Test.bat` | Selects the new executable with full combat, general fields and expanded objects enabled at 384x160. |
| Other ordinary launchers inside full-viewport worktree | Do not themselves enable `SWORDCRAFT3_FULL_COMBAT_RENDERER`; merely choosing that directory does not select full combat. |

The full-frame launcher opens settings and uses a private ROM/save-slot copy plus
an isolated battery save in `build-native/full-combat-playtest`. Its first-run
copy is not ongoing synchronization with the older build's saves. Use the older
launcher to roll back; do not overwrite saves to switch renderer versions.

Some console messages in `tools/launch_custom_native.ps1` still describe only
static general maps or say the game renders at 240x160. Guest coordinates remain
240x160, but that wording no longer describes full-frame combat output accurately.
This is a documentation/launcher-label cleanup item, not proof of a rendering bug.

## 2. What the current renderer actually does

Source of truth: `src/custom_renderer.cpp`, `src/custom_field_scene.h`,
`src/custom_battle_scene.h`, `src/combat_frame_renderer.h`,
`src/custom_battle_state.h`, `src/battle_layer_policy.h`, and the engine's
`src/runtime/host_frame.h` / `host_presentation.h`.

| Situation | Current composition and decision |
| --- | --- |
| Supported combat, full flag enabled | `CombatFrameRenderer` composes the whole 384x160 surface, including the center and HUD. Explicit complete-frame ownership bypasses native-center overwrite. |
| Supported field | Game-owned field compositor draws the extended columns; original native scanout supplies the center. This **is custom widescreen**, but not yet full-frame ownership. |
| Combat without full flag | Retained captured-PPU `draw_view` route, then native-center protection. |
| Dialogue, menus, unauthenticated scenes, unsupported full-frame conditions | Intentional native framing/fallback. Restore waits for a complete captured frame. |

Stock PPU execution and immutable scanline capture still run in the new build.
The extra native correctness redraw and visual comparison path are removed from
the current custom renderer, not merely enabled by an optional switch. The old
`CUSTOM_REPLAY_CHECK=0` launcher assignment survives, but is not consumed there.
The configured build has `GBA_COSIM=OFF`; no independent CPU cosimulator is used.

Battle ownership comes from verified scheduler/lifecycle hooks. Field ownership
comes from task/control/script state and authenticated resources. These decisions
do not inspect HUD pixels. The source/upload-entry consistency checks that remain
are not duplicate image rendering and should not be confused with the removed
correctness redraw.

Game-specific ownership, resources and scenery/effect rules remain in the game
repository. Stateless GBA sampling and the reusable full-frame ownership contract
live in gbarecomp. The field and combat composers still have separate composition
implementations; the current code is not yet one complete shared field/battle
frame pipeline.

## 3. Already implemented — do not restart these from older notes

- General source-backed field maps, rather than only a fixed five-room whitelist.
  Real capture evidence still covers five layouts, not every eligible room.
- Regular field animations resolved from each map's own resources and clocks.
- Guarded field NPC body visibility, resource residency and shadow handling.
- State-based dialogue framing, including box gaps, with narrowly authenticated
  exceptions for field tool strikes and R/L tool selection.
- Combat ownership independent of ordinary R selection, airborne or pause states.
- Authored HUD wings and finite near scenery separated from repeating far scenery.
- Separate arena eligibility and spell-layer permission: an unfamiliar effect
  need not collapse otherwise supported scenery to native framing.
- Reviewed regular/signed/affine spell families, including earlier Aqua Ball,
  critical and Dark Hole corrections.
- Flare Burn movement correction through publication-time spell metadata,
  rather than a spell-name exception or a last-good-image cache.
- Full-frame combat composition and explicit host ownership in the new experiment.

The evidence for earlier fixes belongs to their recorded executable versions.
They are inherited by the new branch, but their entire historical playtest corpus
was not rerun on the full-frame compositor.

## 4. Remaining limits and unverified claims

### Field coverage and composition

`custom_field_scene.h` still skips the native center while drawing. Its existing
offscreen-object loop also prunes native-only objects. A full-frame migration
must address both, not just remove the final center copy.

General mode requires three authenticated text-map sources, a reviewed register/
priority schedule, valid source-to-upload alignment and supported regular
animation metadata. Separate active scripted-animation slots, unsupported blending
and other formats still decline. General mode is **not all-room support**.
Room isolation, arbitrary connected-map extension and camera clamping are not
established by this implementation. Unknown field effect/affine objects are not
generically extrapolated into the margins.

### Combat coverage

The current arena gate supports IDs **0, 2, 3 and 7**. This is a supported-set
count, not the total number of arenas in the game. Full composition supports
modes 0/1 and rejects mosaic-enabled backgrounds or enabled/non-disabled mosaic
objects before drawing. Such cases can still return the entire frame to native
framing; that conservative limit was not removed by the earlier R/jump fix.

BG3 scripted effects remain native-only outside the original viewport. Unknown
BG2 descriptors/windows also lack margin permission. This can clip an effect at
the old screen boundary even while the arena stays wide. No claim of all-spell
or all-summon coverage is justified.

### Performance, stability and distribution

- There is no fresh displayed-FPS/audio benchmark for the approved full-frame
  executable. Earlier headless timings measure earlier builds/routes and must
  not be quoted as its performance. Keeping stock PPU execution also means
  full-frame ownership does not automatically eliminate that cost.
- Historical rewind and chest crashes were outside the renderer fixes. This
  audit did not reproduce or close them. A future translation release fixing
  the chest issue remains an expectation, not current verified resolution.
- Reset/restore state checks do not replace a long save/load/rewind soak test.
- This is a development executable, not a newly packaged release. The old
  `Alpha unfinished-build v.01` package/status record is not this checkpoint.
- The recorded public game repository uses a private engine fork. A new
  contributor needs dependency access; pushing the game branch alone does not
  make a public recursive source checkout independently buildable. No repository
  visibility or access settings were changed or freshly rechecked here.

## 5. What the available evidence proves

Rehashed both executables and read all twelve identity manifests in
`validation/full-frame-20260923`. They identify the correct accepted/full pair,
384px width, intended input sequences and the full-combat switch.

The recorded report contains **838 paired frames**, including **596 completed
full-combat frames**: Flare Burn movement, rocky R cycling, rocky jumping,
critical hit, victory/exit and a field negative control. The report records
unchanged sampled guest state; its documented checks also cover captured
raster/window/source metadata and ownership release. These compare the old
accepted widescreen build with the new widescreen build, not an unmodified GBA
reference. Existing NPC draw/resource hooks already have their own guest effects.

The stored final CTest log has **14 passed selected tests**, including 46,248
composition/source assertions and 31 primitive source/control assertions. It is
not the complete 41-test inventory. Legacy presentation/PNG utility tests are
excluded from this state-only evidence. No new tests were run for this audit.

The owner's “Works great!” is playtest acceptance. It is valuable but is not
exhaustive hardware accuracy, unseen-area coverage, or a measured pacing result.
The most recent full-viewport `playtest-20260923-223925-937` folder contains only
`session-input.trace`; it does not add F10 frame/state captures to this evidence.

## 6. Upstream integration is unfinished and separate

`experiments/upstream-integration/gbarecomp` still has three unmerged index paths:
`src/gba/gba_ppu.cpp`, `src/runtime/runtime.cpp`, and
`tests/ppu_smoke/test_main.cpp`. The UI has two:
`src/common/backends/imgui/launcher_imgui.cpp` and `src/common/launcher_model.c`.
Even if individual file contents were manually resolved, the merges are not
completed in Git. No `build-native/Swordcraft3CustomRendererBeta.exe` exists in
that checkout. Do not describe these dependency updates as integrated, built or
accepted, and do not publish the partial merge as the tested renderer checkpoint.

Finish dependency integration as a separately validated milestone. Mixing it
with field migration would make regression attribution and rollback harder.

## 7. Documentation reconciliation

Historical records remain intact. For present decisions, apply these corrections:

| Older statement or record | Current interpretation |
| --- | --- |
| Widescreen library chapters 03/14/24: optional native replay, three field profiles, forest pilot | Snapshot guidance; current custom code removed replay checks, general fields exist, four arena IDs are authorized. Pixel-based verification suggestions are superseded by the user's state-only instruction. |
| `NO_EXTRA_REPLAY_TEST.md`: opt-in replay, native image comparisons, R-selection still narrow | Historical September 14 diagnostic, not current code or backlog. |
| `GENERAL_FIELDS_20260922.md`: static-only general maps | Superseded by `FIELD_ANIMATIONS_20260922.md`; general regular animations exist. |
| `SELECTION_SPELL_PERMISSIONS_20260923.md`: remaining fire complaint not fixed | The subsequent display-epoch correction addresses the traced Flare Burn movement rejection. Do not generalize that to every fire effect. |
| Earlier spell/field docs: native center always copied | Still true of field and legacy combat; false for accepted complete-frame combat with the full flag. |
| `CUSTOM_RENDERER.md`, `CUSTOM_COMBAT_RENDERER.md`, early handoffs | Historical implementation/coverage checkpoints, not the latest architecture description. |
| `ALPHA_V01_STATUS.md` | Older local packaging candidate, not evidence that the new renderer is release-ready. |
| `FULL_VIEWPORT_COMBAT_20260923.md` | Current combat architecture and evidence baseline; “overworld not migrated” means full-frame ownership, not absent field widescreen. |
| `EMERALD_RENDERER_REFERENCE_20260923.md` | Reference review only. No Emerald code/dependency was imported; it supplies useful source/residency ideas, not the full-frame architecture used here. |

## 8. Recommended agenda after this audit

1. **Make the selected build unambiguous.** At the next approved change, clarify
   launcher labels/routing and point current-status documentation to this audit;
   retain an explicitly named rollback launcher. Do not silently merge/promote.
2. **Migrate field composition to full-frame ownership on the experiment branch.**
   Reuse current authenticated sources, regular animations, NPC/shadow residency
   and state-based cutscene/tool rules. Preserve finite black boundaries and
   intentional native cutscenes. Do not widen AI, collision or camera behavior.
3. **Validate that migration at 12:5**, with all five existing field layouts,
   motion, tool selection/strikes, NPCs/shadows, dialogue gaps, menu/battle
   transitions and restore negatives. Compare sampled state/source/control-flow
   against the accepted build; use owner playtesting for appearance, not automatic
   image assertions. Do not restore the removed correctness redraw.
4. **Measure actual performance/audio pacing**, separating native PPU/capture,
   custom composition and presentation costs. Keep historical headless timings
   labeled historical and avoid assuming the architecture itself improves FPS.
5. **Expand evidence-driven coverage**, prioritizing real unsupported field
   formats and BG3/special combat effects over indiscriminate permission widening.
6. **Finish upstream integration and release preparation separately**, including
   build reproducibility, dependency access and clean-machine checks.

The next substantive renderer task is item 2. It completes the architectural
direction already accepted without treating implemented field support as missing.
