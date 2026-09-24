# Source-backed regular field animations — 12:5

## Outcome and run

Use the owner-folder **Launch General Field 12x5 Test.bat**. Compatible regular
field animations now take frame counts and durations from their own bounded ROM
resources, instead of requiring a named room with four frames at 8 or 14 ticks.
This is a renderer-only change: it does not modify game scripts, camera,
collision, animation timing, simulation, NPC activation or guest code.

The accepted field-tool permission and foreground-script native framing remain.
Authored finite black map boundaries remain; no scenery is mirrored or invented.
The original 240x160 center is copied unchanged into the 384x160 host view. No
same-kernel correctness replay, image classification or framebuffer assertions
were used. There are no engine/UI/submodule changes in this step.

Rollback: close the game and use **Launch Custom Renderer - Overworld and
Combat.bat** without the general-field flag. Its prior known-profile animation
path is retained. The exact prior executable is also preserved privately at
`validation/field-animation-20260922/before.exe`, SHA256
`5e69429f363b9cc98951917de6d5e23f6e3ca8f9eeb63813c5f298c8aff17ec1`.

## Traced animation contract

The field loader `08094A4C` resolves the layer record's animation resource ID
at `+0x1E`, using archive `(1,2)`, and passes its **raw, uncompressed ROM** data
to `08005D6C`. This is distinct from the LZ77 base-map path. `08001DE8` parses
the metadata; `08001D88` defines archive lengths in 16-byte units.

| Resource field | Meaning and provenance |
| --- | --- |
| u16 `+0x12` | Logical byte extent; all six inspected assets end exactly at this extent, with archive padding outside it |
| u16 `+0x14` | Placement count, copied to live descriptor `+0x10` |
| u16 `+0x16` | Family count, copied to live `+0x12`; table shape independently confirmed for all six inspected assets |
| u32 `+0x18`, `+0x1C` | Resource-relative placement list and family table offsets, aligned down to four by the guest |

Each six-byte placement contains tile X/Y, a byte family index and initial
next-frame index. Family pointers are `table + u16(table + index*2)`, not
resource-relative. The four-byte family header specifies byte frame count,
width and height. A frame contains a u16 duration followed by `width*height`
u16 tilemap entries. The duration's low byte is effective; zero is a 256-tick
countdown, since the guest timer is a byte.

In `08005560`, the queued block is selected from `next-1` **before** timer and
index updates. Timers may be observed at zero during reload, and `next` may
briefly equal the frame count before wrapping. Variable-period checks allow
both adjacent durations to account for initialization and reload stores.
These clock bounds do not independently identify the displayed upload phase.

The physical arrays at `03002AF0` and `03002D50` each have 64 bytes. Live
allocation ranges for all four backgrounds, including BG0, must be disjoint,
within the global allocation at `030029B0`, and agree with active/reserved
counts. Rendered layer counts, metadata words and pointers must exactly match
their ROM resource. Header/table/list/family ranges, logical/archive extents,
initial indices and map footprints are checked before use. Overlapping
placement footprints remain unsupported.

The cache holds one scene's immutable source metadata. The key includes ROM
identity, field owner, archive root, three map IDs and three animation IDs.
Ownership loss and host reset invalidate it. Live counters, descriptors,
task/control state and base-map identity are still checked per frame; parsing
is not repeated while the source key remains stable. No history of previously
displayed terrain is retained.

## Phase reconciliation and limits

Each placement independently intersects its candidate frames against ordered
native upload-ring **entries** on captured raster rows. Static entries must
still match their authored source. Disagreement retains native framing. This
is source/register validation, not a pixel or framebuffer comparison.

Candidate preference matches the previous implementation: `next-1`, then
earlier frames. A wholly offscreen or entry-ambiguous placement therefore uses
RAM-clock extrapolation; it is **not** independently observed offscreen truth.
The general path supports 1..255 frames and at most 64 placements. Large-frame
families have synthetic parser/selector coverage, not full raster/performance
coverage; candidate-loop cost for a hypothetical 255-frame map is unmeasured.

Other restrictions are unchanged: three authenticated `0x4000` maps, reviewed
field register/priority schedule, no unsupported blending or active separate
12-slot scripted animations, and valid map-to-upload alignment. This does not
solve interior room isolation or claim all rooms work. Known-profile hashes
remain only for legacy mode and the narrowly verified chief ambient event;
general animations do not borrow a lake/village clock.

## Evidence

Private artifacts are under `validation/field-animation-20260922`.
Built executable: `build-native/Swordcraft3CustomRendererBeta.exe`, SHA256
`c63356f8d4a2f24ba3ebd3454a20f7d1d3286af7097ebf8448bc0c73be5a1eb8`.

- `animation-provenance-checked.json`: 38 saves, 21 field-owned captures,
  all 63 corresponding layer animation sources authenticated. Includes new
  village/lake captures from `playtest-20260922-145648-344`. No placement
  overlap, invalid clocks or counter-allocation conflicts were found.
- Only the existing five field layouts are represented. Live automatic
  animations still use four frames and 8/14 ticks. Other observed resources
  contain 3/5/6-frame families with zero automatic placements: useful format
  evidence, **not** new live-room coverage.
- `regression/report.json`: 1,379 paired frames, both builds in general mode
  with expanded objects. Five fields, two genuine script states, three tool
  action captures, both fresh field captures, menu and arena negative case.
  All recorded RAM/VRAM/palette/OAM hashes, cycles, state metadata and
  wide/narrow decisions match the preserved build. Script states stay native;
  tool states stay wide. Stable fields parse their animation resources once;
  menu ownership loss/reentry causes the expected second decode.
- `native-objects/report.json`: another 374 paired frames across five fields,
  two script states, menu and arena, with original NPC submission. The old
  build's profile mode and new build's general mode preserve guest hashes,
  cycles and field/menu/script framing. Together the two suites cover 1,753
  paired frames. These are state/permission checks, not visual correctness
  certification of every offscreen tile.
- Eight targeted CTests pass: new synthetic animation source/clock/phase
  coverage plus existing field action/source/provenance, lake control/clock,
  battle state and battle identity tests. Synthetic cases include malformed
  archive/logical extents, live pointer substitution, all 64 counters, variable
  and zero durations, 1/3/5/255 frames, same-address resource changes, reset,
  ordered-entry conflicts and all 80 legacy four-frame preference cases.
- `performance/report.json`: three warmed 120-frame lake batches per mode,
  same new executable, headless TCP, width 384, no screenshots/replay check.
  Median processing time: retained profile path **8.65 ms/frame**, general
  animation path **8.53 ms/frame**. End guest hashes match. This small
  difference is not a claimed FPS gain; audio/presentation pacing was not
  evaluated by the headless benchmark.

Source saves were only read and their hashes stayed unchanged. Each test used
an isolated battery save and loopback TCP process. No 16:9/2:1 runs occurred.
The actual JP/beta ROM bytes also agree at the reviewed loader/update routines;
live gameplay tests used the owner's beta edition.

## Separate Aqua Ball report

New battle captures, especially frame 45213 in the same playtest folder,
identify a combat window-policy defect unrelated to field animations. The
spell's BG2 canvas is forbidden outside WIN0 by WINOUT, but the broad authored
scenery-margin bypass still permits it at negative X. Its normal blend enable
remains disabled there, explaining a possible opaque far-left fragment.
This follows saved registers and ordered map entries, not image inspection.

The private `aqua-ball-diagnosis.md` records exact state identities and code
locations. A completed per-row replay trace is still needed to associate the
snapshot conclusively with the displayed artifact. Disabled-BG2 cleanup with
descriptor `4305` is a second potential native-framing interruption. No combat
fix is included here: the next scoped task is to distinguish scenery extension
from spell-window permission and verify the cast/cleanup sequence.

Follow-up completed separately: [Aqua Ball windows and cleanup](AQUA_BALL_WINDOWS_20260922.md)
records the per-row reproduction, per-layer engine policy, scoped game fix and
final state-only regression results. The executable identity in this field
animation report remains the pre-spell-fix checkpoint, not the latest build.
