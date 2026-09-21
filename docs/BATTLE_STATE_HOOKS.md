# Battle ownership and HUD schedule — 2026-09-21

Owner playtest: **accepted** ("Experiment successful!"). Preserve this as the
game-state combat checkpoint on `experiment/custom-combat-renderer-20260913`,
with reusable engine changes on `experiment/sc3-combat-renderer-20260913`.
This acceptance applies to the tested experiment, not universal arena coverage.

This replaces the custom combat renderer's HUD-color recognition and source-map
hash eligibility. User instruction: state/control-flow assertions only, no
screenshots, pixel comparisons, or visual recognition. Test viewport: 12:5,
384x160. The legacy non-custom adapter is not rewritten by this change.

## Source evidence

The local read-only csm3 reference `asm/code_attribute.s` and `src/vcount.c`
identify these relationships, verified against the local English-beta ROM and
TCP memory traces:

| Owner / address | Meaning established from code |
| --- | --- |
| `0x0802B95C` | Battle lifecycle dispatcher |
| `0x03006AB4` | Lifecycle phase: 3 intro, 4 combat, 5 result preparation, 6 escape, 7 result, 8 teardown; 11 scripted variant |
| `0x03006AC0` | Pointer to battle root, `0x03000000` |
| root + `0x0C` / `0x0F` | Main battle mode / pause substate |
| root + `0x1A90` | Battle background renderer object, enabled word at +0 |
| root + `0x1A94` | Arena descriptor index (0 forest, 3 Manig rocky) |
| root + `0x1D2C` / `0x1D2E` | HUD / scenery BG0 control descriptors |
| root + `0x1D30` | Planned VCOUNT boundary, 18 through 58 in steps of 8 |
| `0x08031BC8` | Battle raster scheduler; R0 is background object, R1 variant |
| `0x080289C8` | Pause state machine: initialize, expand, paused, contract |
| `0x080331C4` | HUD panel tilemap writer |
| `0x0805E780` | Rewards-panel initialization, retiring the arena raster schedule |

The scheduler installs four VCOUNT writes: switch BG0 and its vertical offset
after the top boundary, then restore HUD BG0 and zero vertical offset after
row 124. The active interrupt table is double-buffered. The planned RAM value
can be a frame ahead; composition therefore uses the captured register schedule,
not the next planned boundary or the contents/colors of HUD text.

Observed pause modes: main mode 3, substate 1 opening / 2 paused / 3 closing;
normal combat returns to mode 2, substate 0. All six separator ends (19, 27,
35, 43, 51, 59) are covered by the same authored schedule.
The byte called `pause` in diagnostic logs is shared battle substate storage;
outside pause mode it can describe another state machine, such as results.

No ATB-ready flag is claimed here. The current selection is traced at player
attributes +0x166 (root +0x8C0 +0x166). Widescreen eligibility does not depend
on that selection, airborne state, text, charge-bar color, or weapon icon.

## Implementation

`custom_battle_state.h` decodes bounded guest memory. The existing emitted
function-entry hook observes `0x08031BC8` without modifying CPU registers or RAM.
The ROM prologue and scene pointer are guarded; normal ROM identity validation
still belongs to the runtime/configuration. Only arena IDs 0 and 3 are enabled.

Observation establishes scene ownership; ownership persists across video frames
while the game's lifecycle still owns that arena. It is cleared on teardown,
setup/unsupported lifecycle phases, root/arena/enable changes, save loading,
rewind and reset. RAM alone cannot enter battle after invalidation: another
verified scheduler call is required. This is not a frame-count grace period.

Results need an earlier ownership exit than outer phase 8. In csm3
`asm/code.s`, `sub_0805BC10` disables the arena VCOUNT schedule and invokes
`sub_0805E780` as its substate changes from 1 to 2. `sub_0805C568` stops invoking
the arena renderer on that results-panel path. An additional guarded entry hook
at `0x0805E780` therefore retires combat ownership immediately. Without it, a
252-frame input trace found differences in results-menu IWRAM/OAM bookkeeping
because the combat OBJ extension stayed armed; this was caught and corrected
before handoff. Other results variants that still invoke the arena renderer
can establish ownership again through the normal verified hook.

An initial one-call-per-video-frame implementation was rejected by state tests:
the scheduler can execute after scanline zero. Requiring a fresh call before
each frame caused three native fallbacks in the 150-frame R-slot test. The
lifecycle latch resolves that scheduling mismatch without changing guest timing.

`custom_battle_scene.h` retains per-row hardware-layout and source-bound guards,
but no longer hashes full maps or recognizes HUD colors to decide whether combat
exists. It supplies the panel writer's own blank tile entry `0x4080`, upper
separator `0x48A6`, and lower separator `0x40A6` in margin columns. Native HUD
contents stay centered. The original GBA center is copied from native scanout;
it is not rendered and compared as a correctness oracle. The optional extra
native replay check is removed from this custom presentation path.

The reusable `GbaReplayViewPolicy::text_margin_entry` callback lives in the
gbarecomp feature branch. It is instance-local, optional, and only invoked for
text backgrounds outside the native columns. Existing captured tile decoding,
palette, priority and blending remain in the engine. Game addresses and tile
choices remain in the game repository; recomp-ui is unchanged.

## Verification and reproduction

`tools/probe_battle_state.py` uses the included local TCP server for input,
frame stepping, RAM/register dumps, runtime trace and guest-state hashes. It
never requests framebuffer data. It writes to a fresh private validation folder
and isolates battery saves; source save-state hashes are checked unchanged.

`tools/validate_battle_state.py` compares before/after cycles, IWRAM, EWRAM,
VRAM, palette and OAM state, plus decoded battle state. Renderer decisions are
asserted from state logs, not pictures. Tests include R cycling, jumping,
pause/unpause, attacks, critical effects, victory and a field negative case.

The supplied failing pause capture is
`validation/playtest-20260920-143815-749/frame-0000001816-1789929516085/state.gbas`.
Its first state-only replay covered 48 samples / 47 complete frames: all complete
frames took the wide path and all six memory-region/cycle comparisons matched
the previous executable. One incomplete frame immediately after state loading
is excluded from complete-frame assertions.

Non-visual unit targets: `swordcraft3_battle_state_tests` and
`gba_replay_policy_state`. They test ownership, phase invalidation, repeated
video frames, ROM/pointer guards, menu independence, authored tile entries,
callback scope and immutable capture state. Legacy image-comparison suites are
not used for this work.

Local regression evidence: `validation/state-battle-regression-v2/report.json`.
All eight cases passed across **924 sampled frames** (894 battle / 30 field).
Every complete combat frame took the widescreen path; field frames did not
acquire battle ownership. Before/after cycles and every recorded guest-memory
hash were identical, including draw bookkeeping and OAM. R inputs visited all
six selection values (0..5); both jumping cases traversed camera offsets 14..32.
There were 887 complete battle frames, excluding the initial partial frame of
each loaded battle snapshot. These are state assertions, not visual approval.

### Final delivered build

Executable SHA-256:
`37f9f66318325851841947b46ea0ae09a23ddb3a66a8d5e3c7a2f3ede74d038b`.
Both state-only CTests pass. Final input-driven evidence covers **1,224 samples**:

- `validation/state-battle-final/report.json`: seven battle cases, 894 frames,
  887 complete frames all wide; all guest-region and cycle hashes identical.
- `validation/state-battle-final-exit/report.json`: 252-frame victory/rewards/
  exit sequence. The results-init event cancels ownership, and lifecycle reaches
  phase 0. EWRAM, VRAM, palette, cycles and decoded battle state are identical.
  Six IWRAM and six OAM samples differ (seven distinct frames with upload lag):
  two results-panel submissions at X=242 now take the native hidden position
  (240,160). Assertions require native culling of those offscreen submissions,
  unchanged retained OAM tuples/order, and changes confined to source-confirmed
  priority heads, software OAM, count, links and draw records. No blanket IWRAM
  exclusion or gameplay-state tolerance is used. This intentionally differs from
  the old build's briefly widened results sprites.
- `validation/state-battle-field-final/report.json`: 30 lake frames, no battle
  ownership and all guest-state hashes identical.
- `validation/state-hud-delivery/`: original failing capture, 48 samples, all
  guest-region/cycle hashes identical to `state-hud-baseline`; all 47 complete
  frames wide, including pause animation. Source save states remain unchanged.

The earlier failed logs are retained as diagnostic evidence, not passing tests.
`--verify-existing` can reassert the stored inputs only when current executable,
source-state and input-sequence identities match the evidence exactly.

## Use / rollback / limits

Use `Launch Combat Renderer Test.bat`, selection **3 (12:5)**, from this checkout.
The build is `build-native/Swordcraft3CustomRendererBeta.exe`. The prior binary
is preserved privately at `validation/state-hud-before/before.exe`; launch with
the normal project/toolchain environment, not by moving DLLs into Windows.

Close the combat test and use the main project's field-only lake launcher to
disable combat expansion. No save migration or camera/collision change is made.
Other arena IDs remain deliberately native.
State tests do not establish visual approval, universal weapon/effect coverage,
or support for every map. A full natural battle-entry and long charged-skill
playthrough remains a broader integration check.
