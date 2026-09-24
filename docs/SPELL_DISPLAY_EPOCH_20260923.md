# Moving spell display-state synchronization

Custom renderer only, 12:5 (384x160). Fixes the timing mismatch documented in
FLARE_BURN_MOTION_DIAGNOSIS_20260923.md without a spell-ID exception, relaxed
position tolerance, or changing guest execution. Native scanout is still
copied into the center; no duplicate correctness render is enabled.

## Ownership boundary

The reference main loop at 08001BC0 calls 080044E4 (display/window shadow
publication), then 08004B30 (scroll publication), before input and subsequent
actor/effect updates. The existing generated function-entry observer now
copies the required IWRAM/EWRAM prefixes at 080044E4. Its 0x60-byte routine
fingerprint matches JP and beta and is part of the supported-ROM gate.

Each raster row decodes against this owned metadata and that row's actual
captured IO. Exact scroll, window, owner, dimensions, format and permissions
checks remain unchanged. Each publication replaces the metadata, even if the
new owner is unsupported/retired. Reset/load/rewind invalidates it; until the
first publication the prior strict live-state decoder is the conservative
fallback. This is not a last-good-window cache. No retained guest pointers,
historical map substitution or frame-count grace period is introduced.

The shared path covers reviewed script-kind 2 and casting-kind 3/4 windows.
Unknown/affine effects retain their existing policy. Publication capture is
about 40 KiB per call, not a copy of all memory on every scanline.

## State-only verification

- 1,222 synthetic assertions: unchanged registers with next-position RAM,
  both directions and larger updates, mismatched publication rejection,
  owner replacement, reset, invalid buffers and casting actor motion.
- All 13 game-side CTests passed.
- Four 100-frame TCP movement replays: neutral, left, right, alternating.
  Each contains 89 active Flare Burn frames. Rejected-window frame counts
  change from 0/17/8/24 to 0/0/0/0. Stationary policy is unchanged.
- Nine earlier spell/field captures, 60 frames each, preserve guest hashes,
  cycles, endpoint metadata, raster registers, arena ownership and framing.
  No previously accepted effect window is lost. The two field cases are
  unchanged. Three spell cases recover single-row retirement timing gaps;
  the casting case recovers one full frame's window eligibility.
- Total paired capture coverage: 940 frames, all at 384x160. Validation
  asserts source permissions and callback reachability, not visible pixels.
  No independent CPU oracle, FPS or audio improvement is claimed.

Evidence is private under validation/flare-burn-fix-20260923. Reproduce with
tools/probe_battle_state.py and tools/validate_spell_display_epoch.py using
the identity.json input sequences/settings. Existing captures are untouched.

Built executable SHA256:
ef8ca3cb9dec56166abe85b852af3f7a97ee87a5151a4ea38a1dddae6847538e

Rollback executable: validation/flare-burn-fix-20260923/before.exe,
SHA256 1b952a23efa5c8c5083a2e30c43da01dbc91775a9d420cd37c1a92b66d119c27.

This checkpoint precedes upstream dependency integration. It does not claim
the dependency updates have been built or accepted.
