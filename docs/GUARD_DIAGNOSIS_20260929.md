# Guard lag and Select/width investigation

Diagnosis only, 2026-09-29 UTC (September 28 local). No gameplay/runtime fix,
rebuild, installed executable replacement, commit or publication in this turn.
The Swordcraft workflow required state/control-flow evidence, not screenshots.

## Findings

1. The Guard package's saved launcher preferences have `smooth_filter=1` and
   `host_aspect_index=2` (384x160). The regular Portable Beta has smoothing off.
   Matched windowed measurements reproduce a substantial Smooth 2x cost, with
   essentially unchanged guest-processing time. This is the primary measured
   explanation for the reported combat slowdown, not proof about every encounter.
2. The Guard action is combat-only, but the experiment's callbacks remain
   installed throughout play. The bus-read path checks exact instruction PCs;
   eligible RAM writes also enter an optional callback. Actual remapping requires
   the authenticated battle root, lifecycle 4, mode 2, non-link subtype, unpaused
   manual control, and (for ability substitutions) the player and held Select.
   This is ongoing checking, not continuously executing the Guard action.
3. Mine slot 5 reproduces Select-triggered native-width fallback in the original
   portable executable, new executable with Guard off, and new executable with
   Guard on. All 97 per-frame guest-state hashes and renderer decisions match
   across those three runs. No Guard substitution occurs in the field.
4. Slot 3 is a native-paused battle in arena 8. The renderer supports arenas
   0, 2, 3 and 7, not 8. It rejects this state with `unsupported-battle-state`
   before and after native Start unpauses it, with both old and new executables.

## Displayed combat timing

Same English combat snapshot, 384x160 host / 240x160 guest, 3x window, Raw color,
effects off, neutral exclusive replay, SDL Direct3D11 and real audio. Each case
runs 600 frames with 90 warm-up intervals excluded; two passes reverse case order.
Guard/state traces and self-heal compilation are disabled for timings. Tests run
serially after the user's game closes. This is not an in-battle input-latency test.

| Case | Pass 1 FPS | Pass 2 FPS | p95 interval, ms |
| --- | ---: | ---: | --- |
| Original portable, Nearest | 58.50 | 59.29 | 18.94 / 18.29 |
| New executable, Guard off, Nearest | 59.25 | 59.40 | 18.05 / 17.94 |
| New executable, Guard on, Nearest | 59.32 | 59.37 | 18.06 / 18.18 |
| New executable, Guard on, Smooth 2x | 46.79 | 46.14 | 23.57 / 23.93 |

Guard-on guest processing averaged 14.63–14.93 ms/frame across the Nearest and
Smooth cases. Presentation rose from about 0.94–0.97 ms to 6.36–6.37 ms with
smoothing; the frame budget was exceeded. No filter-resource fallbacks occurred.
The existing filter performance document already records this widescreen cost.

Separate uncapped TCP runs (three 180-frame batches per case, restored and warmed
each time; two reversed-order passes) did not show consistent Guard-on overhead:
neutral off 16.41–16.57 ms/frame versus on 16.04–16.17; native B-Guard
15.89–17.49 versus Select-Guard 14.31–16.56. These noisier processing-throughput
samples are **not displayed FPS**, do not establish a speedup, and cannot rule out
small hot-path costs. They do not support blaming a large slowdown on Guard.

## Select and renderer ownership

Slot 5 begins in a valid 79x77-tile field, flags 1, battle phase 0. Thirty neutral
steps produce thirty complete wide field frames. At the Select pulse (step 31),
field flags become 4 and the field VM starts script processing; widening is
rejected as `lake-player-control`. The next 67 observed frames remain rejected.
A 97-frame neutral control stays completely wide. This is **not** an observed
one-frame-only blink; the exact partial-wide/misaligned-center appearance remains
unverified and needs the relevant movement/event sequence.

The read-only csm3 reference identifies the native path: `080947B0` tests the
Select new-press bit, takes the field's script ID at offset +6, and calls
`080A4564`. That routine clears free-control bit 1 and sets script bit 4. The
observed changes match that path. The Guard hook does not intercept these PCs.
The field renderer intentionally refuses most script states. Host composition
keeps its requested 384-wide surface but uses the centered 240-wide native image
when widening fails; this is not Select changing the user's aspect preference.

The known supported combat fixture separately compares old B-Guard, new B-Guard
with the mod off, and new Select-Guard. Player records match across all 97 steps;
all 96 completed captured frames have complete wide ownership. No additional
fallback occurs at Select press/hold/release. Slot 3's resumed comparison also
matches player records and ownership decisions, but all 129 captured frames
reject arena 8. Its short input sequence does not establish active Guard coverage
in that arena; do not count unsupported-renderer fallback as a widening test.

## Evidence and identities

Source baseline: `96c0a3eaf52ec30372eaf7e9715deacc35f8f145` plus the existing
uncommitted Guard experiment. Executable SHA-256:

- Original Portable Beta: `c1570c220a5cda6d62e0cb1415cb06fa05ef5b6b5fd2b11369264e9f08733deb`.
- Delivered Guard engine: `63598812f4bdd6fbb2eea11754448d0fcc42597633adbfc3d7896504652eae73`.

Private evidence directories under `validation/` (do not publish):

- `guard-diagnosis-window-1790651863290234700`: cadence and phase CSVs, eight runs.
- `guard-diagnosis-throughput-1790652029132552200`: uncapped comparison.
- `guard-diagnosis-field-1790651782794370400`: first slot-5 comparisons.
- `guard-diagnosis-field-1790652587460422600`: repeated slot-5 comparisons with
  corrected diagnostic working-directory isolation.
- `guard-diagnosis-field-1790652225928094600`: paused slot 3.
- `guard-diagnosis-battle-1790652279838152100`: supported battle guard comparison.
- `guard-diagnosis-battle-1790652388242036600`: slot 3 resumed with native Start.

All reports confirm the supplied state, ROM, BIOS and executable hashes unchanged.
The original `Session` helper ran from the executable directory, causing generic
ROM/BIOS pointer sidecars and coverage reports to be written under both installed
Runtime folders. Those eight current diagnostic artifacts were moved recoverably
to `validation/guard-sidecar-recovery-20260929`; nothing was deleted. The helper
now runs in each private test directory, and slot-5 comparisons were repeated.
The portable launcher's real `Settings/rom.cfg`, `Settings/bios.cfg` and launcher
preferences are separate; no player preference or save was intentionally changed.

Follow-up correction: the [short playtest](SHORT_PLAYTEST_20260929.md) established
that cwd isolates coverage files but not generic asset-picker pointer sidecars,
which follow the executable directory. `Session` now explicitly supplies a
private portable data root as well. The additional two diagnostic sidecars were
retained recoverably outside the game folder; subsequent full-folder hash checks
passed. Do not rely on cwd alone to isolate installed-engine tests.

## Next steps (not implemented)

- Immediate user-controlled performance workaround: Nearest or Sharp, effects off.
- Optimize Smooth 2x's host presentation cost before promising full-speed 12:5.
- Investigate arena 8's authored assets and renderer support as a separate change;
  do not simply remove its state guard.
- Handle the relevant field-script transition coherently after authenticating
  which script states should remain wide, preserving actual menus/cinematics.
- The experimental Guard callbacks can be narrowed to exact input/action sites
  or combat lifetime for efficiency, with state-load/teardown tests. Measurements
  do not currently show them causing the large slowdown.
