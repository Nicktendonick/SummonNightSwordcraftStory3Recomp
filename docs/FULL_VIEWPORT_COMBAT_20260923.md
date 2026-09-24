# Full-viewport renderer migration: combat checkpoint

## What changed

This is a separate experiment, not a promotion of the accepted build. Game
branch: `experiment/full-viewport-renderer-20260923`, based on `1af07ad`.
Engine branch: `experiment/full-viewport-primitives-20260923`, based on
`73a001e`. The recomp-ui dependency remains unchanged at `92fc0ae`.
The paused upstream-integration worktree is not used by this build.

The SuperMetroidRecomp review (upstream `14cfc9e0dee56f9abb7a23eb5bc85cefcd843a9a`)
established the relevant distinction: its game renderer composes the entire
supported gameplay image, while stock PPU execution/capture and intentional
HUD/unsupported-scene fallback still exist. That architecture is the reference,
not SNES hardware code or a claim that no native PPU work remains.

`CombatFrameRenderer` now owns every column of each supported combat frame,
including the center and HUD. It samples captured GBA text/affine backgrounds,
OAM, palette, windows and blend controls, and resolves one output surface.
It does not instantiate a replay PPU or call `draw_view`/`render_scanline`.
Existing game-owned scene-source callbacks are reused as a policy adapter;
they choose authenticated map/window continuations, not a second rendered image.

The shared `HostFrameOwnership` contract lets a successful callback claim the
whole surface. Without that explicit per-call claim, legacy native-center
protection remains the default. Failure, denied presentation and reset retain
native fallback. All three former center-overwrite sites are bypassed only
for successfully completed full-combat frames. Counters distinguish these
frames from `native_reused`; neither counter claims visual verification.

Generic stateless tile/OAM/source-address/priority/effect primitives belong in
gbarecomp. Combat eligibility, authored scenery bounds, effect placement and
HUD continuation remain in the game repository. No Super Metroid source was
copied, no ROM-derived generated output was edited, and no guest hooks changed.

## Contract and limits

- Target: 12:5, 384x160; existing reviewed arenas 0, 2, 3 and 7.
- Center contract: direct composition from the same captured hardware state,
  not a stock framebuffer pasted over the center. No pixel-equality assertion.
- Native PPU execution and capture still run. No extra native validation
  render, cosimulator, guest step or frame comparison was added.
- Captured hidden affine references are resolved at the observer's pre-reload
  boundary, not approximated from live next-frame registers.
- No simulation, camera, activation, collision, timing, resource or save-format
  changes are intended relative to the accepted custom build.
- Existing scene ownership, spell display epochs and source authorization
  remain in force. This does not make every spell/arena universally supported.
- Modes outside 0/1 and active mosaic are not migrated. The complete frame is
  rejected before drawing and uses native framing; unknown scenes do likewise.
- The overworld is deliberately **not migrated yet**. Its accepted rendering
  and dialogue framing remain the legacy route in this checkpoint.
- This is an experimental compositor, not independently certified GBA hardware
  accuracy. Source/control checks cannot certify its appearance or audio pacing.

## Verification

Build: `build-native/Swordcraft3CustomRendererBeta.exe`.
SHA-256: `c29226f597d3a5fb0e4480f0db4cc43dc3ec348c676b6c9bcf5e4bbc0024215b`.
The private fast build verified unchanged cached guest source, rebuilding only
the two already configured hook shards. It is not a release/distribution build.

Fourteen selected CTests passed (exclude `presentation|widescreen_route_audit`).
The new compositor suite made 46,248 ownership/source assertions; the new
engine primitive suite made 31 address/priority/effect/ownership assertions.
The legacy PNG-audit utility self-test was initially included by a broad test
filter; it is excluded from the final state-only validation and is not evidence
for this renderer. No gameplay framebuffer was requested or compared.

`tools/validate_full_combat.py` performed isolated TCP input runs against the
accepted executable and rechecked their recorded state/raster metadata:

| Sequence | Paired frames | Complete combat frames | Affine frames |
| --- | ---: | ---: | ---: |
| Flare Burn left/right changes | 100 | 98 | 0 |
| Rocky arena R cycling | 150 | 149 | 0 |
| Rocky arena jump | 186 | 185 | 0 |
| Critical hit | 90 | 89 | 7 |
| Victory and exit | 252 | 75 | 0 |
| Field negative control | 60 | 0 | 0 |

All 838 paired frames preserved sampled guest hashes/cycles and captured
raster/window/source-permission metadata. Every completed frame accepted by
the combat route was full-frame-owned, with 38,400 center and 23,040 extended
columns composed. Ownership was released on exit and never claimed for the
field control. Initial restoration frames still wait for complete captures.
Private evidence: `validation/full-frame-20260923/report.json` and case files.
These are state/route results, not visual approval or displayed-FPS measurements.

## Try it / rollback

Run `Launch Full Frame Combat 12x5 Test.bat` in this experiment folder. Settings
open before launch; F10 captures remain available. On first launch it copies
the existing battery save and available state slots into an isolated private
playtest folder (the runtime associates slots with the ROM path, so a private
ROM copy is used too). Existing saves are not overwritten.

Close it and use the usual custom-renderer launcher to roll back. The accepted
executable remains SHA-256
`ef8ca3cb9dec56166abe85b852af3f7a97ee87a5151a4ea38a1dddae6847538e`.
At the initial test handoff, nothing had been pushed, merged into the playable
branch, or released. The owner subsequently reported "Works great!" and
authorized publishing the tested progress. This records owner playtest approval,
not exhaustive coverage of every battle effect. The checkpoint retains its
separate feature branch and does not include the unfinished upstream integration.

Next: migrate the field compositor to the same full-frame ownership model,
with broader battle-effect and fade testing as new cases become available.
