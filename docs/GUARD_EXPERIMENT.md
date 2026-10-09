# Select-to-Guard experiment

**Superseded:** the approved feature is now integrated into the normal portable
launcher with a saved Mods toggle. See [Guard mod release](GUARD_MOD_RELEASE_20260929.md).
The text below records the original experiment. Environment-only recipes do
not override the saved choice in current portable engines.

Local experiment after source checkpoint `96c0a3e`; not a release update or a
GitHub push. Launch with the separate `Swordcraft Story 3 Guard Test.exe`.
The ordinary beta starter clears the opt-in environment flag, including when
both starters are in the same private test folder.

## Player contract

- In normal, manually controlled combat, Select feeds the native B input path
  with Guard selected **for that action only**. Hold to guard, release to stop;
  this is not a toggle. If physical B is also held, the native held-B behavior
  still applies. Existing attack, jump, stun and other action gates remain.
- The selected R ability and its HUD cursor/label stay selected. Ordinary B
  continues using that selection. R still cycles it when explicitly pressed.
- Select does not turn auto-battle on in this experiment. A state with existing
  auto-battle enabled can still cancel it with Select through the original code;
  manual Guard can then engage. A/B's native auto-cancel behavior is unchanged.
- Field/menu inputs, scripted and link-battle variants retain native behavior.
  The allowed gameplay difference is the declared input/action remap; there is
  no invulnerability, damage, collision, camera, rendering or timing override.
- This uses the existing Select binding for keyboard and controller. It does
  not add a production Mods toggle or an independently bindable extra action yet.

## Implementation and evidence

The read-only csm3 reference and both owned ROM revisions identify the same code:

- `080272E2`: the manual player-input read. Substitute Select with held B before
  `080412D8` computes native press/release/history buffers. No per-frame tap loop.
- `08029966`: mask Select only at the auto-battle enable check. The already-on
  cancellation read at `0802999A` is deliberately untouched.
- `08042698` / `080426DC`: the player's action-local ability selector reads see
  zero (Guard); the stored ability selector is not changed by these reads.
- `08042862`: native Guard would reset that selector to zero. The optional
  RAM-write value seam preserves its existing byte, at this exact instruction,
  address and actor only. The instruction/write/timing path still executes.
- `08042866`: supply a non-player comparison sentinel to the local equality
  test, skipping the redundant HUD-reset calls. This value is never dereferenced
  and the actual battle-root pointer is unchanged.
- `08049018`: native held-B continuation/release is completely unmodified.

`src/guard_experiment.h` authenticates five complete surrounding code ranges,
verified identical in Japanese and supported English. Runtime whole-ROM checks
remain in force. State gates require the exact battle root, lifecycle 4, mode 2,
normal substate, manual control and player actor. Unknown code/state falls back
to native behavior. There is no host-side saved selector or guard latch to leak
through load-state, rewind or battle teardown. Generated files are not edited.

The reusable seam is `gbarecomp/src/runtime/ram_write_override.h`, used by the
three CPU bus-write helpers and reset on run initialization. It supports only
bounded physical IWRAM/EWRAM writes and clips the replacement to the write size.
It cannot alter MMIO, DMA, ROM or save writes. Game addresses remain in the game
repository; recomp-ui is unchanged.

## Validation

- Both language engines and the separate Windows starter built successfully.
- Nine focused CTests passed: Guard, presentation preferences, portable language,
  runtime menu, portable launcher, portable input, battle window, battle state,
  and runtime monolith boundary.
- Guard unit tests cover all 1,024 GBA input combinations, six slots, lifecycle,
  actor/address/width/site negatives, code-layout rejection and generic RAM-write
  seam bounds/null/reject behavior. Both real ROMs passed the fingerprint check;
  mutating a guarded instruction was rejected.
- `tools/validate_guard_experiment.py` passed 20 cases / 2,017 recorded frames.
  Native B-Guard and Select-Guard had byte-identical 0x390-byte player records,
  including hold/release timing, also for the airborne and pause/release cases.
  Actual R inputs visited all six slots; Select guarded without changing any
  selection. Existing AI cancellation and restoring/releasing a held-guard state
  passed. Ordinary inputs and field Select produced identical guest-state hashes
  with the mod enabled versus disabled.
- Japanese was independently cold-booted with its own ROM, never with an English
  snapshot; 366 frames per mode matched all guest-state hashes. Japanese combat
  has not been playtested, despite matching code fingerprints.
- Repeated the complete 2,017-frame suite against the exact stripped executables
  in the delivered test folder, with matching results and unchanged inputs.
- Packaged launcher preflight and automated open/normal-close passed. Copied
  executables retained identical loaded sections after debug stripping and
  passed DLL-dependency closure checks.

Private evidence: `validation/guard-1790649028658867700` (build) and
`validation/guard-1790649401899335400` (packaged executable identities included).
These assertions use state/control flow, never pixels, screenshots or framebuffer
comparisons. English cases exercised existing RAM interpreter fallback routes
with self-heal compilation disabled; no fully-static gameplay claim is made.
Japanese cold boot reported no dispatch misses. The tests are not a performance
benchmark, exhaustive damage/weapon/enemy coverage, or a physical controller
playtest. Host Esc/focus-loss and actual rewind button feel remain manual checks;
native Start-pause and save-state restoration were checked above.

Follow-up user-playtest diagnosis: [Guard lag and Select/width investigation](GUARD_DIAGNOSIS_20260929.md).
Matched combat benchmarks measured about 59 FPS with Nearest and 46–47 FPS with
Smooth 2x at 12:5, without a consistent Guard-on regression. Mine slot 5's Select
fallback also occurs in the original portable build; slot 3 is unsupported arena
8. These are findings, not implemented renderer/filter fixes.

## Private test package / rollback

Delivered folder, relative to the owner project:
`release/Guard Experiment 20260929-023325-a2fa`.

- Guard starter: `Swordcraft Story 3 Guard Test.exe`.
- Native comparison: `Swordcraft Story 3 Beta.exe` in the same test folder.
- Save-state slot 2: known English combat fixture. Slot 1: copy of the user's
  existing English slot 1. Separate settings and language save banks.
- `TEST-PACKAGE.json` records copied input and binary hashes. This is a **private
  folder containing owned ROM/BIOS copies**, not a distributable ZIP. Do not upload.
- Close other Swordcraft instances before opening either starter. Return to the
  original `release/Portable Beta` to leave the experiment entirely; it was not
  modified. No source commits, pushes, release replacements or tag moves occurred.

Reproduce with `tools/package_guard_experiment.py`; it always creates a fresh
folder and verifies that its source inputs were unchanged. The opt-in runtime
switch is exactly `SWORDCRAFT3_SELECT_GUARD=1`; other values leave it disabled.
`SWORDCRAFT3_GUARD_TRACE=1` enables diagnostic logs and is off for normal play.
