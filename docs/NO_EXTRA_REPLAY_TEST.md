# Combat without the extra native redraw — 2026-09-14

**Current default:** after reviewing the explanation and fresh timings, the owner
requested that normal play no longer use this technique. The extra native
redraw is now OFF by default, including the usual combat launcher. Only an
explicit `SWORDCRAFT3_CUSTOM_REPLAY_CHECK=1` enables the developer diagnostic.
The comparison launcher remains compatible but is no longer required to get
this behavior. The original opt-in comparison below is historical.

User-requested reversible comparison. Run `Launch Combat - No Extra Replay.bat`
in this experiment directory and choose a width. Close the game and run the
ordinary `Launch Combat Renderer Test.bat` to restore the extra check. Both use
the same experimental save location. Neither launcher permanently changes the
environment or save format.

## What the correctness check actually did

This build has `GBA_COSIM:BOOL=OFF`. No independent emulator or second guest CPU
is executing alongside normal play for this check. Each visible scanline is
captured with immutable VRAM, palette, OAM, IO and affine state. The normal
native frame is then redrawn with the same GBA pixel kernel and compared to the
live scanout before the expanded combat frame is rendered. That is a renderer
capture/replay consistency check, not an independent accuracy oracle.

The earlier 16.3 ms measurement was TOTAL frame processing with the capture and
extra native replay enabled, but combat widening disabled. It was not the cost
of the check alone. Native-only was 10.3 ms in that earlier measurement; the
approximately 5.9 ms difference includes capture, replay and related overhead.

## Opt-in change

`SWORDCRAFT3_CUSTOM_REPLAY_CHECK=0` skips `draw_native` and copies the live native
scanout as the reference image. The default is still enabled; the normal combat
launcher explicitly enables it, and the new comparison launcher disables it
only within `setlocal`.

Still present:

- The real native game frame and immutable raster capture.
- The expanded battle render, layer composition and HUD-border checks.
- Exact comparison of all native-center pixels in that expanded render against
  the live native frame, plus host-presentation consistency checks.
- Existing scene recognition, culling and fallback behavior.

No frame skipping, simulation-rate change, cached scenery or additional arena
authorization was introduced. Logs distinguish checked `matches` from
`native_reused`; reuse is not reported as a successful replay comparison.
All changes to this switch are game-owned. No reusable engine modification was
needed for this request.

## Validation

Build log: `validation/build-no-extra-replay.log`.
Executable SHA-256:
`7adb9b62b6b3e97b279aeb0025eed1d8e045e5b3c9d2c806a9aa302c35a11238`.

`tools/validate_field_objects_tcp.py --feature replay` compares OFF/ON using the
same isolated snapshot, inputs, native/host screenshots and state hashes. It
asserts equality of the ENTIRE host picture (including margins) and all sampled
guest-state hashes, not just the original center.

Passed runs in `validation/no-extra-replay-*`:

| Case | Host width | Compared frames |
| --- | ---: | ---: |
| normal | 384 | 60 |
| critical | 384 | 90 |
| pause | 284 | 60 |
| r-selection | 384 | 30 |
| village | 384 | 60 |
| chief | 284 | 60 |

All 360 frame pairs are identical, with matching guest-state hashes and source
snapshots unchanged. The R-selection case remains narrow in both modes: this
confirms unchanged behavior, not a fix. Seven focused CTests also pass.

The known R-selection identity bug and unsupported arenas remain separate work.
This switch is not yet the default and has not been pushed or released.

## Paired processing benchmark

`tools/benchmark_combat_tcp.py --compare-replay` uses the same captured forest
battle, three warmed 240-frame batches per mode, and excludes startup, save
loading and screenshot transfers. `validation/no-extra-replay-performance`
records median frame processing of **20.255 ms with** the extra redraw versus
**16.510 ms without** it at width 384 (about 18.5% less processing time).
This is headless throughput, not measured window/audio pacing; live 60 FPS
still requires owner testing, and the margin against the frame budget is small.

## Clean rerun and default change

`validation/no-extra-replay-retest-20260914` reruns five warmed 300-frame batches
per mode, after confirming the other game instance was closed. Internal
scanline profiling is disabled and the mode order is reversed (OFF first).
Median headless processing is **15.213 ms/frame OFF** versus **17.857 ms/frame
ON**, about 14.8% less processing time. These results supersede neither earlier
measurements nor their variability; they are a fresh paired comparison on the
same executable. Screenshot transfer and final state hashing are outside the
timed batches. This still does not measure window presentation or audio pacing.

Normal play now uses the measured OFF path by default. The existing native
frame and expanded frame are still rendered; the third, redundant native redraw
is not. Native-center/HUD/scene checks that operate on already-produced data
remain. The same-renderer replay can test capture completeness, per-line memory
and hidden affine-state reconstruction, but cannot detect a decoding bug shared
by both paths. It was a temporary bridge-consistency diagnostic, not independent
emulator validation, and no independent emulator was running alongside play.

Default-OFF build SHA-256:
`d6e3651d767e3fe497dc5df178f01bf52373f2e60b42b58966ab089a88fa28a5`.
`validation/replay-default-off-smoke` verifies 60 wide combat frames with the
environment switch absent: startup reports check OFF, `matches=0` and
`native_reused=60`. The complete final image matches the earlier no-replay
reference; all seven focused CTests pass. The benchmark above used the preceding
binary with explicit OFF; the subsequent code change only changes its default.
