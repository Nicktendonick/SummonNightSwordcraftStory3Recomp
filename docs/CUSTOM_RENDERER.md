# Custom-renderer adaptation — native bridge and lake host surface

Current scope: three authenticated outdoor scenes (opening lake, village-chief
outdoors and the adjoining village outdoors). Use the same top-level
`Launch Custom Renderer Lake Test.bat`; its menu now says lake and village.
All other maps and battles remain native in that launcher. An opt-in
[forest combat pilot](CUSTOM_COMBAT_RENDERER.md) is available separately through
`Launch Combat Renderer Test.bat`. Conversations retain native framing
until player control returns. Milestones below are historical; the final
section describes the village expansion and its validation. The subsequent
[NPC graphics-residency correction](NPC_RESIDENCY_FIX.md) supersedes the original
offscreen-NPC validation claims and documents the distorted-sprite regression.

The owner reported the native build good and stable on 2026-09-10. The next
milestone adds a separate, fixed-width host surface and opening-lake scenery.
Use `Launch Lake Renderer Test.bat` in this worktree (or the top-level
`Launch Custom Renderer Lake Test.bat`) and choose 16:9 first. The existing
native-test launcher remains available for comparison. Both lab launchers
share only the experimental playtest save, not the ordinary project's save.

Game branch: `experiment/custom-renderer-20260909`.
Engine branch: `experiment/sc3-custom-renderer-20260909`.
Both are isolated worktrees under the owner's project `experiments/custom-renderer`.
The starting game checkpoint is `210aa41`, engine `f85f0bc`, UI `92fc0ae`.
The tree-decoration and unfinished camera-limit work remain in the original
checkout and are not part of this experiment.

## Import versus adaptation

Upstream SuperMetroidRecomp commit `3b419d00dd29696947fc311991c7c59a5b9861b7`
is vendored, with its noncommercial license and provenance, under
`third_party/supermetroid-renderer`. Those SNES-specific files are reference
sources, not linked into the GBA executable. They are not hardware-independent.

The implemented first adaptation is the immutable raster capture/replay
architecture: each of the GBA's 160 visible scanlines owns its IO, 96 KiB VRAM,
palette, OAM and hidden affine accumulator state. Replay runs in a separate
PPU instance with capture/presentation callbacks disabled. Guest execution,
audio, timers, input and memory are not advanced by replay.

The **existing native GBA pixel kernel is reused**. Passing the native oracle
therefore validates capture timing, isolation and plumbing, not an independent
new pixel decoder. A complete-frame oracle compares replay against stock
before substitution; mismatch or incomplete capture retains the stock image.
The feature defaults off and enabling it deliberately caps the guest to
240x160. A separate host surface may extend the presentation only. No old
widescreen hooks or guest OAM-culling overrides are enabled in this mode.
No interpolation or new presentation cadence is enabled yet.

Shared native capture/replay plumbing belongs in `gbarecomp`; the game adapter
and its enable/fallback policy belong in `src/custom_renderer.*`. No recomp-ui
source changes were needed.

## Build and validation

Run `tools/build_custom_renderer.ps1` on the owner's current Windows setup.
It uses a separate `build-native/` tree and one compiler worker. For iteration
speed, the lab target links the existing unchanged English-beta generated guest
objects from the main checkout, but builds its engine/host separately. This
private shortcut is NOT a reproducible release package or substitute for a
fresh ROM-verified code generation build before distribution.

`gba_native_capture_tests` covers all six GBA video modes with per-line changes,
repeat drawing after caller-memory mutation, incomplete/out-of-order capture
rejection and output-size guards. It passed on 2026-09-09.

`tools/validate_custom_native.py --output-dir validation/<new-name>` compares
native captures and recorded guest state for the old executable, new-off and
new-on, and requires active successful custom replay diagnostics. It uses
released-control continuations from lake, battle, critical-hit and dialogue
snapshots. Inputs remain read-only; all outputs and scratch saves are private.
Integration results are recorded below.

### Native integration checkpoint

`validation/native-check-1/report.json` passed all four scene comparisons:
lake, normal battle, critical hit and dialogue. Seven sampled frames per scene
(28 total) matched pixel-for-pixel and in the recorded guest-state fields across
previous/new-off/new-on runs. Each on-run logged at least 60 successfully
replayed native frames with zero pixel mismatches. Battle, critical and dialogue
runs each used one stock fallback for the initial incomplete save-state frame;
the next complete captured frames used replay. This is expected on mid-frame
loads and is not counted as a custom-rendered frame.

Final checks on 2026-09-10: `runtime_monolith_guard`, `gba_native_capture`, and
`ppu_smoke_tests` all passed (3/3). The lake and critical-hit native output
images were also visually inspected. The tested executable SHA-256 is
`C13AF5548A31CE4CE352FB6DFD47E56034DA5040ECE2189979DCD2E699D5E0E7`.

Use `Launch Native Renderer Test.bat` in this worktree, or the owner's top-level
`Launch Custom Renderer Native Test.bat` shortcut. It uses the lab's own battery
save and settings. Native 240x160 is intentional for that comparison launcher.
The launcher path check does not certify UI
interaction or sustained live audio/performance. Closing the lab and using the
ordinary launcher returns to the existing executable. No GitHub push was made.

## Remaining adaptation, in order

1. Play-test the new lake surface in motion. The owner accepted native stability;
   this is not yet acceptance of wide performance, live resize or all lake exits.
2. Broaden scene coverage after the lake. The separate host surface and owned
   lake maps are implemented; unsupported scenes intentionally remain native
   inside black margins. Profile before making a performance/60-FPS guarantee.
3. Capture correctly phased sprite-owner data and verify it against native OAM
   before reconstructing any omitted objects/effects. Do not spawn enemies early,
   widen gameplay activation, or guess that every wrapped object is real.
4. Test the lake and first arena, then add an optional translucent dialogue
   margin treatment. This filter is not part of the native bridge milestone.

This architecture cannot invent missing map art, infer all room/exit boundaries,
or automatically port Super Metroid's object/effect addresses to Swordcraft 3.
Those are game-specific reconstruction tasks. Native correctness is not proof
of widescreen correctness or whole-game compatibility.

## Lake host-surface checkpoint — 2026-09-10

`SWORDCRAFT3_CUSTOM_RENDERER=1` selects native capture/replay.
`SWORDCRAFT3_CUSTOM_HOST_WIDTH=284`, `320` or `384` additionally selects the
fixed wider host surface. Window resizing scales/letterboxes that chosen
surface; it does not yet change its logical width. The lake launcher prompts
for width before opening the usual settings launcher. F10 saves a wide PNG,
native guest snapshot and metadata under this worktree's `validation/playtest-*`.
It also records input there. Existing snapshot/TCP and final headless dumps
remain native; delivered window frame dumps use the host dimensions. The F10
report distinguishes `view_width` (guest) from `host_width` (presentation).

The runtime's `HostPresentation` restores all native center pixels after the
game callback, including when it declines or tries to overwrite the center.
It never expands guest PPU geometry. The game adapter checks replay against
native scanout, and invalidates its completed host image on snapshot load or
rewind. It copies full source maps at scanline zero and draws with each owned
scanline's VRAM, palette and scroll registers. All three original 360x320 lake
map hashes must match; empty/out-of-map areas remain black. No tree decoration,
mirror/repeat fill, camera change or object activation change is introduced.

Animation required additional game-specific reconstruction. Local csm3
`asm/code_08004544.s` routines `sub_08005560` and `sub_080057E4` describe the
six-byte placement records, animation tables and deferred tilemap writes.
The tested lake has four-frame, 14-tick families. RAM animation indices alone
were not aligned with visible VRAM, so a constant timing guess caused periodic
fallbacks. That checkpoint resolved a **unique shared phase** against all
native source/ring entries for the complete captured frame. Ambiguous phases,
unknown animation layouts, active scripted animation slots, UI, windows,
unsupported blending and stale map data decline the entire extension.
This confirms the tested phase mapping, not all possible animation families.

Validation on this executable (SHA-256):
`921091A77DE234E170BE37581B710B9C6DE63BF02251FBE0AA8618EE046763CD`.

- `validation/host-lake-acceptance-1/report.json`: 18 wider-view comparisons
  across idle lake, walking lake, dialogue entry, battle, critical hit and
  dialogue at 284/320/384 pixels. Each checks 91 consecutive native-center
  images and 91 full guest-state records (including PPU and aggregate hash).
  All match native. Idle/walking lake runs have no intermittent fallback;
  dialogue entry declines only for screen UI. Unsupported scenes stay black.
- `validation/native-host-regression-1/report.json`: the previous executable,
  new-off and native replay still match the four original scene fixtures.
- Four CTests pass: native capture, host presentation, PPU smoke and runtime
  monolith guard. Host tests exercise odd/even widths, fallback, center
  preservation, bounds, and all eight text-map size/color-depth combinations
  against stock PPU pixels (307,200 pixel comparisons).
- Actual lake, walking and dialogue output images were visually inspected.
  The launcher input-path check passed; live launcher interaction and sustained
  wide audio/performance have not been certified.

This remains a **scenery-only pilot**. Characters/particles culled by the guest
are not reconstructed in the new columns. Battles, other maps and dialogue
filtering remain next steps. The original game launcher/build is untouched by
this custom-renderer milestone. No GitHub push or release was made.

## Lake flicker and cutscene-control correction — 2026-09-10

The owner's `playtest-20260910-123237-241` exposed four rejected frames in a
301-frame continuation. The old whole-map phase assumption was false: separate
animation placements can reach the visible ring at different times.
`validation/flicker-baseline-2/report.json` records the original black flashes.

`CustomLakeScene` now intersects phase candidates **per animation placement**.
All visible cells of that placement must agree, and every static source cell
must still equal the captured ring. Overlapping/unknown animation layouts fail
closed. No previous rendered frame is held or stretched to conceal disagreement.
Completely offscreen placements, or phases indistinguishable in the native
viewport, prefer the most recent completed RAM phase (`next - 1`). That part
is an explicit extrapolation, not proof of invisible hardware pixels.

Cutscene framing now reads the actual field flags used by csm3
`asm/code_small_structures.s`, `sub_08093994`. The field pointer lives at
IWRAM `0x03006B54`; its first halfword has player input bit `0x0001`, foreground
script bit `0x0004`, and input-block bit `0x1000`. Widening requires
`(flags & 0x1005) == 1`, plus the existing authenticated lake maps and raster
checks. Dialogue-box visibility is still an additional safety gate, but no
longer the sole authority. This preserves native framing between boxes and
allows reopening when foreground scripting ends and input is enabled.
The pointer is bounds/alignment checked and no guest flags are written.

An earlier attempt to recognize a particular script ID / VM call stack was
discarded in favor of these input-dispatch flags. `dialogue-control-1` started
from a BIOS-wait snapshot and did not advance the text; it is not evidence of
dialogue progression. `dialogue-control-2` advanced a resumable event and showed
box-free scripted actor movement, but had not reached restored player control
at its endpoint (VM waiting on actor movement, opcode `0x0441`). Neither run
certifies a complete cutscene-to-free-roam transition.

`tests/custom_lake_control_test.cpp` covers enabled control, foreground scripts,
input blocking, pointer bounds/alignment, unrelated flags and immediate release
when the input flags return. Build it through `tools/build_custom_renderer.ps1`.
The five focused CTests (lake control, native capture, host presentation,
PPU smoke, runtime monolith guard) pass.

The capture replay and host validation tools now run the existing lab executable
in place, avoiding extra 230 MB executable copies. Do not rebuild during a run.
The five intermediate `host-lake-check-1` through `host-lake-check-5` executables
were moved, at the owner's request, to
`Documents/Codex/Discarded Files/SummonNightSwordcraftStory3Recomp/` for the owner
to delete. Accepted builds, captures, reports and saves were not removed.

This remains lake-specific presentation. Battles still fall back to native
because this renderer has no battle reconstruction yet; no stuck cutscene
latch is carried into battle. The ordinary build is untouched. A live check
of dialogue completion and restored movement is still needed before accepting
the cutscene behavior across all lake events.

Final build SHA-256:
`EC9B86F162F2BECEA5E833A0181C3C3F76117D5AF48204AB5B88DFE61E96644D`.
`validation/host-lake-control-flags-acceptance/report.json` passed all 18
scene/width comparisons (1,638 native-center and full-state comparisons).
`validation/flicker-control-flags-384/report.json` passed 601 consecutive frames
on the owner's flicker route at 12:5 with no rejected frames or black flashes,
and exact native-center/full-state parity. The same animation correction had
already passed 601 frames at 16:9 in `validation/flicker-final-284/report.json`
before the final field-flag refinement. One previously flashing 12:5 frame was
also visually inspected. These checks do not certify unseen routes or sustained
live performance. No commit, push or release was made for this correction.

## Rare one-frame animation-clock rejection — 2026-09-10

The owner's later `playtest-20260910-185523-555` captured frame 17155 just
after another flash. Replaying the recorded input from the owner's earlier
slot 4 reproduced a single rejected frame at 17019, recovering at 17020.
The native crop at the replay endpoint is byte-identical to the actual F10
capture, confirming this route. The save was read only.

`validation/rare-line-route-1/report.json` records the failing 356-frame range;
`validation/rare-line-diagnostic-2/384/raw/native/composite/stderr.log`
identifies the rejected placement: base 6, index 22, next 2, timer **0**,
position (31,28), size 2x1. This was not a camera-boundary or cutscene failure.
The renderer rejected the complete added margin for one frame.

Local csm3 `sub_08005560` explicitly stores zero while decrementing the timer
before reloading its 14-tick duration. It also briefly stores next=4 before
wrapping to zero, after reloading the timer. A frame-start snapshot can land
between these instructions. `custom_lake_animation.h` therefore accepts
timer 0..14 with next 0..3, plus next=4 only with timer=14. The latter state
is assembly-supported and unit-tested, but was not the observed flash.
All other clock states and unsupported families still fail closed. ROM
bounds, authenticated source maps, placement bounds and per-placement native
ring matching are unchanged. No stale-frame caching or guest writes were added.

`tests/custom_lake_animation_test.cpp` exhaustively tests byte-sized counters,
rejects unsupported frame counts and checks the four-candidate index range.
All six focused CTests pass (animation, field control, native capture, host
presentation, PPU smoke and runtime monolith guard).

The owner reports the conversation framing worked in live play. The village
chief's item-giving event and other unseen cutscenes still need live testing.
This correction does not broaden the pilot beyond lake scenery or remove
intentional black space beyond the finite map. The ordinary build is unchanged.

Corrected executable SHA-256:
`F859E53406A337DFE9585A795E5850DBE41B68A36E429A57BD52B6385D04BAE5`.
`validation/rare-line-fixed-284/report.json` and
`validation/rare-line-fixed-384/report.json` each pass 356 consecutive frames
(16800..17155) with uninterrupted wide coverage and exact native-center/full
guest-state parity. Both runs retain 906 intentional `lake-player-control`
declines earlier in the replay, before the sampled free-roam range; neither
has an animation-descriptor decline. Report `passed` alone checks parity:
the continuous `margin_changes` and decline reasons were checked separately.
Comparing all 356 complete 12:5 images with the old build found exactly one
changed image: the repaired frame 17019. Its restored margin was visually
inspected. Both 16:9 and 12:5 launcher path checks pass; the usual
`Launch Custom Renderer Lake Test.bat` points to this experimental executable.
`validation/host-animation-clock-acceptance/report.json` also passes all 18
scene/width comparisons (1,638 frames), preserving native pixels and complete
guest state for idle/walking lake, dialogue entry, battle, critical hit and
dialogue. Unsupported scenes retain native framing. No commit, push or release
was made; broader live cutscene and sustained-performance acceptance is pending.

## Village outdoor profiles — 2026-09-10

After accepting the repaired lake, the owner supplied
`validation/playtest-20260910-232912-511`:

- Frame 16024: free movement outside the chief's house; three 512x320 maps,
  no tilemap animations, player-control flags `0x0001`.
- Frame 16799: chief dialogue on the same map; control flags `0x0004`.
- Frame 18812: adjoining village outdoors; three 512x400 maps, two BG1
  animations (four frames, eight ticks, 5x5 tiles), control flags `0x0001`.

The last scene also matches the older frame-33123 village capture. No room
interior is assumed to be supported merely because it shares map dimensions.

The game adapter is now `src/custom_field_scene.h` (`CustomFieldScene`), with
three explicit entries in `src/custom_field_profiles.h`. Geometry only chooses
a candidate: every layer must also match its allowlisted SHA-1. Animation data
must match the selected profile's family, and all visible static/animated
entries must match the native raster ring. Map dimensions and row strides are
profile-driven; storage is bounded to the largest verified 64x50 tile map.
Unknown maps, animation families, effects and invalid descriptors still fail
closed. The native-screen UI checks and player-control gate are unchanged.
The legacy `lake=` diagnostic counter and `lake-player-control` reason now
cover these three field profiles; their names remain for existing audit tools.

All changes are game-specific and stay in this experiment's game repository.
No additional engine/UI changes, guest writes, camera clamping, scenery
wrapping, decorations or omitted-sprite reconstruction were introduced.
Black space beyond real map bounds is intentional. Only previously verified
background priorities/layouts are admitted. The chief profile has no animation
placements; the village's eight-tick animations use the same independently
reconciled placement logic and transient timer/index handling as the lake.

The six focused CTests pass. Byte comparisons against the accepted lake build
also confirm all 546 complete lake/walking images at 284/320/384 are unchanged,
including the extended margins, not just the native centers.

Candidate executable SHA-256:
`2877111B6C140353E081EC6D2A4559D17151163BF1588F58176560D8231582E3`.
The new scene fixtures were added to `tools/validate_custom_host.py`.
`validation/chief-village-route.trace` is a private segment of the owner's
recording from the frame-16024 snapshot through frame 18812, excluding the
later backwards timestamp caused by a save load. The original recording and
all snapshots are untouched.

The first expanded matrix (`validation/host-village-acceptance-1/report.json`)
stopped after 21 passing comparisons: its rightward `chief_walk` fixture
actually triggered the chief's automatic greeting. Native framing was correct;
the test's always-wide expectation was wrong. The corrected harness separates
`chief_approach` (exactly one wide-to-native change) from a short upward
`chief_walk` away from the trigger. Village free-walking similarly avoids the
adjacent conversation trigger. This is a test correction, not relaxed scene
authorization; the executable was not rebuilt.

`validation/chief-village-route-384/report.json` checks a 2,781-frame recorded-
input continuation with 279 image samples and 2,781 full guest-state records.
All sampled native centers and complete recorded states match the native-only
baseline. The chief conversation closes the wings, they reopen on returning
to free movement, close for the map transition and reopen in the village, then
close for another NPC conversation. All intermediate map/animation validation
declines occur inside already-native periods. Frames showing these states
were visually inspected. However, the replay's endpoint is in that next
conversation, unlike the owner's frame-18812 still. It is therefore controlled
continuation evidence, not an exact reproduction of the entire live session
or certification of every future cutscene.

`validation/host-village-acceptance-2/report.json` passes the remaining 15
comparisons with corrected fixture expectations. Together with the 21 completed
comparisons from the first run, all 36 unique scene/width cases are verified
on the same executable (3,276 native-image/full-state frame comparisons).
The first run's overall `passed=false` is retained honestly; the entire matrix
was not rerun after this harness-only correction. Six focused CTests pass.

`validation/village-camera-sweep-384/report.json` additionally passes 241
consecutive frames of a longer movement probe from the older post-event
village save. It has no renderer declines or margin dropouts, with native
pixels and full guest state identical. Its output was visually inspected.
The existing top-level lake-test shortcut resolves to this tested build, and
both 284/384 launcher path checks pass. The owner should now test free movement,
chief dialogue and transitions live. Other areas, omitted offscreen sprites,
performance guarantees and a release remain outside this checkpoint.
No commit, push or release was made.

## Field NPC visibility and TCP validation (2026-09-12)

The experimental launcher now extends regular field sprites as well as scenery
in the three authenticated outdoor profiles (lake, chief area, village).
Battle and dialogue framing remain native. This is not a general all-game or
all-sprite-mode implementation.

There were three separate restrictions:

1. The host field compositor drew only backgrounds. It now samples regular
   OBJ pixels from immutable per-scanline OAM/VRAM/palette, using OBJ/BG priority
   and OAM order. Authored black margins stay above sprites. Affine, mosaic,
   semitransparent and OBJ-window sprites deliberately decline in the margins.
2. The existing reviewed immediate seam at 08009B9E/08009BB4 now widens the
   per-piece draw-list cutoffs while a supported controllable field is active.
3. `sub_0809FB0C` performs an earlier actor-versus-240px-viewport test. Expanding
   only the per-piece cutoff was insufficient: the old woman still disappeared
   when completely outside the native view. Exact-PC bus-read overrides at
   0809FD38 and 080A0070 extend only the later draw-list visibility reads, using
   the live actor rectangle retained at sp+0x10. They validate the original
   viewport rectangle and do not alter the earlier shared rectangle test or
   stored gameplay visibility flags. The current control gate and authenticated
   chief ambient-event exception apply to both sprite hooks.

Normal negative OAM X values are decoded using the extended culling range,
including sprites whose last few pixels touch the left host edge. This game
adapter is capped at 384 host pixels; still wider views need an unwrapped
coordinate sidecar to disambiguate overlapping positive/negative OAM encodings.
The existing launcher choices (240/284/320/384) are unchanged.

Reusable code is in the engine experiment branch: immutable regular OBJ sampling
(`gba_obj_sample.h`) and TCP host presentation support. Game-specific PCs,
eligibility, bounds and composition policy remain in this game repository.
No recomp-ui changes were needed.

### Validation and limitations

`tools/validate_field_objects_tcp.py` launches isolated localhost TCP sessions,
loads owner-provided snapshots through `savestate_load`, sends `set_keyinput`
and `step`, and reads both `screenshot` and the new `host_screenshot` command.
The latter returns the actual custom host composition; the original command
still returns the untouched native oracle. TCP initialization now publishes the
same renderer inputs as normal play. Host screenshots require a completed frame
and park the core before composing. Sessions quit cleanly and use private battery
saves. Captures, command logs and memory dumps are ignored validation artifacts.

Final build SHA-256:
`a21ea37c6bdc1a9eaf581c77fc5c3c6109828f37e06be452983b7b576682716f`.

Final TCP evidence under `validation/tcp-field-objects-final-*`:

- village: 90 frames, 384px, left held then released. The old woman remains
  visible when fully outside the original right edge. All native-center pixels
  and cycle counts match the disabled-feature comparison. The final four EWRAM
  differences (F074/F07C/F07D/F08C) are within that NPC's drawing structure;
  OAM, graphics transfers and draw bookkeeping also differ as expected.
- chief: 60 frames, 284px, upward movement. NPC pixels extend past the native
  edge; native-center pixels and all measured memory-region hashes match.
- dialogue: 30 frames, 284px. Entire host images and measured state hashes match
  the feature-disabled run; the margins remain black.
- lake: 60 frames, 320px, rightward input. Entire images/state hashes match;
  this is a scenery/control regression, not evidence of a new offscreen NPC.
- Six focused CTests cover control/ambient gating, animation, left/right object
  bounds, regular OBJ sampling, immutable captures, host presentation and PPU
  smoke behavior.

These checks do not prove every NPC, attachment, overlapping sprite, map edge,
long cutscene or live audio/performance situation. Left wrapped-X math has unit
coverage; the current live NPC captures primarily exercise the right edge.
Full guest-state equality is no longer the expected invariant when additional
draw records are submitted. Older scenery-only parity tools can be run with
`SWORDCRAFT3_CUSTOM_OBJECTS=0`; do not relabel rendering-memory differences as
full-state parity. This environment switch also restores the previous
background-only host behavior, without reverting other renderer fixes.

Use the existing `Launch Custom Renderer Lake Test.bat` in the main project
folder to try the updated executable. No commit, push or release was made.

## Chief ambient-event flicker — 2026-09-11

The owner's `playtest-20260910-234758-171` supplies frames 17286 and 18010.
A continuation of the first snapshot reproduces a black-margin dropout at
17704..17706, reopening at 17707. This is a different cause from the lake's
animation-clock fault: field flags briefly equal `0x0004` during a short event.
The map and native ring remain valid. Source investigation confirms the same
flag is used for real dialogue, so ignoring it generally would be wrong.

Audit-only control and VM logging identifies script 201, VM base `0x02006000`,
slot 0 active, at opcode `0x033c` / cursor `0x02006cde` and opcode `0x0001` /
cursor `0x02006cee`. Local opcode-table inspection maps these to
`sub_08014C00` (audio scheduling through `sub_080137C0`) and `sub_080128BC`
(variable/flag assignment). The adjacent `0x040b` handler `sub_080A6A30`
restores player input. These specific observed sound/flag states are not
treated as cinematic presentation.

`custom_field_events.h` recognizes ONLY these two VM states, with flags exactly
4, script 201, the expected base pointer and active-slot mask. The renderer
also requires the authenticated chief outdoor profile and SHA-1
`6f29204f532fbe80bf81ebbe22f278a7d2f2cb29` over the 64-byte event region at
`0x02006cd0`. Other opcodes, cursors, scripts, changed bytes, extra lock flags
or maps cannot use the exception. Native UI/effect and ring checks still run.
This adds no timeout, cached picture, guest write or broad event bypass.
Negative state tests cover unrelated scripts/flags/slots/opcodes/cursors.

Investigation artifacts:

- `chief-flicker-followup-384`: the original strict replay checker stopped
  because menu transitions omitted four PNGs (17427, 17499, 17570, 17646).
  A separate wide diagnostic was completed: both widths omit the same four
  images, all 721 available native crops match, and full state traces match.
  This does NOT make the original incomplete replay a full passing test.
- `chief-flicker-approach-384`: slot 4 stayed in an earlier lake cutscene;
  it did not reproduce the owner's approach and is not acceptance evidence.
- `chief-control-diagnostic` and `chief-event-diagnostic`: reproduce the
  17704..17706 dropout and identify the control/VM states above.
- `chief-event-fixed-384`: 91 consecutive frames 17680..17770 stay wide with
  exact native-image/full-state parity. Earlier menu declines are outside
  this checked interval. The repaired frame 17704 was visually inspected.

The NPC cutoff is a separate known limitation: `custom_field_scene.h` draws
background layers only, and the native bridge deliberately disables the old
guest OBJ-culling overrides. Restoring NPCs requires host-side character draw
reconstruction and correct foreground/boundary masking, not merely adding
more map profiles or enabling all legacy widescreen hooks. No NPC restoration
or engine change was made in this correction.

Final tested executable SHA-256:
`FBD2C79C18527B384EA94066CB5F4B3C5A21D94385FC62100373ED827A59444F`.
Both `chief-event-fixed-284` and `chief-event-fixed-384` pass the 91-frame
flicker interval with continuous wide coverage. Complete-picture comparison
with the original 12:5 replay changes only frames 17704, 17705 and 17706.
`chief-event-regression/report.json` passes all 12 scene/width comparisons
(1,092 native-center/full-state comparisons): lake, chief approach, actual
chief dialogue and village movement at 284/320/384. The six focused CTests
pass, and the existing lake-test launcher resolves to this executable.
This fixes the reproduced event, not every possible unseen foreground script.
No commit, push or release was made.
