# Follow + edge stops — private portable experiment

September 30–October 1, 2026. Game checkout: `experiments/full-viewport-renderer`.
This is a separate opt-in camera experiment, not a published release or an
automatic update of `release/Portable Beta`.

## Visible behavior and contract

`Current`, `Bounded`, and `Follow + edge stops` are separate saved choices.
Launcher Mods exposes two mutually exclusive experiment checkboxes; neither
selected means Current. Esc > Graphics > Battle framing exposes all three.
The clean-install default is Current. The private test defaults to Follow.

Follow keeps the original centered widescreen anchor whenever it fits inside
the near-scenery source. Near either end it translates all gameplay layers,
objects, signed windows and affine sampling together. It never writes the guest
camera, actor positions, collision, AI or perspective state. HUD rows stay
centered. Original native-view callback coordinates remain unchanged after
accounting for the host translation; no framebuffer oracle is used.

At 384x160 (12:5), a fixed 346-column opening has a 19-column border/shadow
frame on each side. At 284x160 (pixel-aligned 16:9), the full width is used.
There is no zoom or nonuniform stretch. The original 240-column view always
fits inside the opening. Native width bypasses the experiment.

This constrains source coordinates, not opacity. Intentionally transparent
tiles are still transparent; no scenery is generated or synthesized to fill
them. Aesthetic approval remains the user's playtest decision.

## Reviewed scope and fallback

Only existing authenticated arenas 0, 2, 3 and 7 are eligible. Their authored
near maps are 384x160 even where the VRAM allocation is 512 columns. Exact
Japanese, English 1.0.5.f and English 1.0.6.f ROM identities are allowlisted,
in addition to the original camera-code/table guards. This startup hashing
does not run each frame.

Framing uses the completed, owned raster's actual BG0 scroll registers across
enabled scenery rows, never next-frame RAM or sampled colors. The October 1
revision keys its anchor to the uniform upper scenery band (rows below 96),
which is independent of ordinary jump height. Normal captured camera mode/
position and conservative reviewed schedule bounds are required.
Special/locked cameras and unreviewed schedules retain Current framing.
Unsupported scenes retain their established native fallback. Results, field
return, restoration, width and preference changes invalidate retained ownership.

The authenticated `080352F4` scheduler has ordinary horizontal C=0..128,
vertical V=0..32, and perspective threshold d=128 or 136. Its upper-band offset
is `Hflat = C + 8 + floor((d-152)*(C-64)/64)`. Integer rounding can alias C
values, so the host unions every matching C and both thresholds. It includes
every vertical state and scenery row up to 124. Conservative monotone bounds
remove one-column reversals where the candidate sets change. No guest scroll
register is reconstructed or written. The table is built once (142 small
entries); the per-frame pass still reads the completed raster.

For opening `[B,E)`, near-source width 384 and stable envelope Emin/Emax:

```
low    = max(B, E + Emax - 384)
high   = min(E - 240, B + Emin)
anchor = clamp((host_width - 240) / 2, low, high)
source_x = screen_x - anchor + row_HOFS
```

The actual captured Hmin/Hmax must fit the stable envelope, and all enabled
upper-band rows must agree on Hflat. Otherwise framing falls back to Current
with the same widened OAM decoding. The envelope still has Emin >= 3,
Emax <= 141 and spread <= 29. Independent full-row arithmetic tests cover every
ordinary camera and vertical value, both thresholds, jump invariance and
monotone horizontal movement.

The exact-ROM near-map cap is also applied in Current/Bounded: reviewed arenas
0/2/3/7 have 384 authored columns, not 512. This policy is separate from the
2/7 additional-area toggle and backdrop-repeat policy, which remain unchanged.
Only extended near-source sampling changes; the native perspective and far
background remain intact. Authored width is not an opaque-art boundary:
transparent scenery/pillar cutoffs and the final desired viewport width remain
unresolved. This revision does not remove the side frame or synthesize terrain.

## Object submission and allowed differences

Only the two established OBJ drawing cutoff hooks change. Additional draw-list
bookkeeping is allowed; actors are not spawned or activated. Follow uses a
constant union of all admitted anchors, also containing Current fallback.
At 384 the origins are -166..342 (509 integers); at 284 they are -104..280.
Each interval fits inside the 512-value OAM X representation with no aliases.
The compositor uses a matching fixed unwrap, including on Current fallback.
HUD objects keep their original decoding. No previous-frame anchor prediction
is involved and no generated C++ is hand-edited.

## English 1.0.6.f

The new patch is applied to the verified original Japanese ROM using BPS
source, target and patch checksum validation. Output SHA-1:
`6753a22a096b8adaa3a869333b99fcfe29ba1fec`; CRC32: `C76631A9`.
It differs from 1.0.5.f by one byte at ROM offset `0x17f32ea`.
`Swordcraft3Translation106` has its own regenerated guest corpus;
`Swordcraft3Japanese106` retains the Japanese corpus. The normal packaging
recipe now selects this pair, but building it does not replace the installed
1.0.5.f portable engines. Older release targets remain for private comparison.

English saves use `Saves/english-1.0.6f.eep` and
`Save States/english-1.0.6f.state*`. The runtime still rejects save states bound
to another ROM. Diagnostic fixture transplants, if used by the private audit,
are explicitly recorded and are not a general user-state conversion feature.

## Reproduction

- `tests/battle_follow_camera_test.cpp`: exhaustive admitted metadata combinations,
  clamp/source bounds, fixed opening, native-view containment, object union and
  unique decoding, negative/fallback inputs and exact-ROM authentication.
- `tests/combat_frame_renderer_test.cpp`: translated callback coordinates,
  immutable captured inputs, HUD exclusion and decoration dispatch counters.
- `tests/battle_camera_preferences_test.cpp`: migration, precedence, persistence,
  mutually exclusive launcher choices and malformed/save-failure handling.
- `tools/audit_translation106_inputs.py`: private ROM/code audit and explicitly
  isolated diagnostic-fixture preparation; leaves all originals unchanged.
- `tools/validate_follow_edge_camera.py`: paired old/new Current, old/new
  Bounded and new Follow routes, both camera limits, jump, Guard, pause/resume,
  restore, entry, result and field return. Native and unsupported controls are
  included. Comparisons use guest memory, input and captured-source metadata.
- `tools/validate_stable_battle_camera.py`: same-ROM old/new comparisons within
  each camera mode, stationary and moving edge jumps, exact recorded guest-state
  hashes, jump-independent host anchor, all four supported arenas, native/284/384
  widths, unsupported arena, legacy renderer, restore, fresh entry and exit.
- `tools/update_camera_test_f10.py --camera-evidence <REPORT.json>`: requires a
  passing full stable-camera matrix tied to the exact source/binaries/inputs,
  then smoke-tests and updates only the private Camera Edge Test engines with
  verified rollback. Keeps F10 capture, settings/saves and main portable intact.
- `tools/package_follow_edge_test.py`: separately staged runnable test, local
  relative inputs, dependency audit, actual launcher/menu/boot smoke checks,
  source/main-install hashes and proof-gated delivery; no publication.

Private evidence and game-derived fixtures belong under `validation/`, never
Git. Neither unit tests nor these finite input routes certify every boss, spell,
weapon, enemy combination, long playthrough or visual result.

## Completed local evidence

- All 27 selected C++ regressions and 15 Python fixture-safety tests passed.
  The exact new English and Japanese ROM guard checks passed; deliberately
  corrupted ROM data was rejected.
- `validation/translation106-input-audit-1790824852616961300/AUDIT.json` passed:
  one changed byte (09 -> 0C), all 35 generated files equivalent under only the
  sixteen explicitly listed operator-label renamings, 43 reviewed code/data
  spans and 35 near/far arena-map inputs unchanged. Ten private fixtures differ
  only in their exact-ROM header identity; all originals stayed unchanged.
- `validation/follow-edge-1790825410595720300/REPORT.json` passed the full
  54-case matrix: 33,330 frame-by-frame input/state observations, plus three
  battle-result/field-return routes. Eligible Follow cases reached both camera
  limits at 284 and 384 columns. Captured-source bounds stayed valid, Guard
  engaged/released, jump and pause/resume behaved normally, and restoration
  discarded old ownership immediately.
- New Current and Bounded matched the installed old engines' recorded guest
  memory/cycle hashes. Follow matched the declared player/gameplay contract;
  native-width and unsupported cases also matched the complete recorded hashes.
  Additional object-drawing bookkeeping is permitted only in eligible Follow.
- Review caught an optional legacy-replay mismatch: the wider Follow object
  union needed the same full-compositor enable gate as its decoder. This was
  corrected before the final matrix. The three added legacy-replay cases now
  match Current's recorded guest state exactly.
- Whole-folder hashes confirmed the main portable installation and every
  protected input remained unchanged throughout the matrix. No screenshot or
  framebuffer assertions were used. The full matrix tests English 1.0.6.f;
  Japanese is separately authenticated and receives package boot/reset checks.
- `validation/camera-edge-20261001-034435-9f84/SMOKE.json` passed with the actual
  packaged launcher: 1.0.6.f BPS import and generated-cache identity, Japanese
  patch-off selection, Follow/Bounded/Current mutual exclusion and reopening,
  English/Japanese sibling-engine handoffs, cold boot, Esc graphics and clean
  reset/close. Both languages reported zero missing static dispatch targets.
  The first smoke attempt mistakenly supplied a menu-bypassing `--frames`
  flag; the corrected run used real launcher PLAY and Esc Close confirmation.
  Failed-run evidence is preserved separately; no game code change was needed.
- The delivered private test is `release/Portable Camera Edge Test`. Settings
  and saves are independent copies. A 1.0.5.f battery save is copied once into
  the new 1.0.6.f filename, but its saved progress has not been playtested; the
  player should check Continue. Old user save states are preserved unchanged,
  not converted. The original portable installation remains untouched.
- The test is not a distributable archive: it contains the owner's private
  inputs. No Git commit, push or publication was performed.

## Stable horizontal framing update — October 1

Implemented the Ghidra investigation's jump-independent host framing and the
exact-release authored near-map cap. All game simulation, native row-scroll
perspective, OAM submission ranges, HUD placement, far-layer policy and field
widescreen behavior remain unchanged. No generated guest code or ROM is edited.

Evidence for this revision:

- All 27 selected C++ regressions and 15 Python fixture-safety tests passed.
  The camera test checks 3,404 admitted source-bound combinations, all ordinary
  camera/vertical states for both thresholds, horizontal monotonicity, OAM
  decoding, invalid-input fallback and the actual English/Japanese ROM guards.
- `validation/stable-camera-1790879690778224900/REPORT.json` passed 25 paired
  same-ROM scenarios, eight paired independent left-edge jump routes, and two
  paired result/field-return routes: 42,264 recorded state observations. The
  comparisons cover English 1.0.6.f, four supported arenas, all three widths,
  Current/Bounded/Follow, unsupported arena 1, legacy replay, restore and fresh
  battle entry. Every paired recorded guest memory/cycle hash matched.
- In rocky arena 3 at width 384, the old Follow anchor moved 23..41 at the left
  edge and 103..121 at the right during stationary jumps. The new anchors stayed
  exactly 22 and 122 respectively across the 90-observation jump/landing windows.
  Native row offsets still varied with height; the guest perspective was not
  frozen. Actual captured source bounds and their stable envelopes stayed valid.
- Earlier failed probes are retained: adding left jumps before crossing altered
  enemy-collision timing, so stationary edge tests now restore separately and
  the right test waits until the ordinary crossing/jump route reaches the edge.
  No enemies, HP or movement state were edited to force these assertions.
- `validation/camera-stable-update-20261001-185216-1fee/REPORT.json` confirms
  installation into `release/Portable Camera Edge Test`. Only its English and
  Japanese engines changed; every other file and the entire main Portable Beta
  matched their before hashes. Old engines are verified in that folder's
  `rollback/` directory. Loaded executable sections are unchanged by stripping.
- Packaged English launcher PLAY, pause/resume, running/paused capture and close
  passed. Japanese boot and capture passed. Both boot smoke runs had zero missing
  static dispatch targets. Battle fixtures still exercise existing interpreted
  RAM routines; this is not a fully-static or performance certification.

The workflow kept the experiment on owned raster state and required guarded
fallback and state-only comparisons. There are no screenshot/pixel assertions,
no visual approval, no all-boss/spell certification, and no new audio benchmark.
The 346-column opening and its side frame remain. Transparent pillar art cutoffs
are not established by the 384-column asset header, so a final pillarbox/art-edge
design is still separate work. No main release update, commit or push was made.

Source delta for this revision is confined to `src/battle_follow_camera.h`,
`src/custom_battle_scene.h`, `src/custom_battle_state.h`, `src/custom_renderer.cpp`,
their three affected camera/compositor/state tests,
`tools/validate_stable_battle_camera.py`, `tools/update_camera_test_f10.py`, and
this document under the custom-renderer checkout. Existing unrelated edits were
preserved.

## Player capture review — 2026-10-01

Read-only review of Camera Edge Test capture `20261001-145850-131-41760`:

- The log selects Follow, Current, Bounded, Follow, Current, Bounded, Follow.
  Successful custom battle-frame counts continue through the switches, ending
  at 5,393 with `reason=none`. This confirms rendering engaged, not visual
  correctness or every-frame continuity.
- Its one GBAS v2 snapshot authenticates as English 1.0.6.f. At frame 7,160 it
  records arena 3, battle phase 4, camera C=128 and V=32, movement limits 24/360,
  and player X=352.80859375, Y=152: near the right edge with the native camera
  already at its right limit. Host width is 384 and guest width is 240.
- The input trace records guest buttons, not timestamped host framing changes.
  There is only one F10 snapshot and no continuous anchor trace. Therefore this
  bundle alone cannot prove the entire run was bounce-free or certify either
  pillar-art cutoff. No screenshot/pixel assertions were made.
- The session reports existing interpreted coverage (`NOT_STATIC`, eight
  dispatch misses), so it is not evidence of fully static execution. The log
  alone does not establish a new regression or the cause of those misses.

Capture SHA256 identities (original files were not changed):

- session.log: `50f45c6d9b66177d26f503354becf7446066b074bb9c2802e112ca736068a097`
- session-input.trace: `c0aa8c89979792bf5a7f1dac043ee336425d348229d241492849a66085879210`
- state.gbas: `4d97c12956b5bd8c57477086dfd2f331b8017c00e8eea68f9f235cdfb838d63b`

The separate Escape-to-Resume fix and test-only deployment are recorded in
`docs/ESC_MENU_20260926.md`. No additional camera policy was changed in this pass.

## Optional rocky-arena scenery cover — October 1

The user approved Follow's movement and requested a trial to hide the scenery
cutoffs near the pillars. `Cover scenery edges` is a separate, default-Off
switch in launcher Mods and Esc > Graphics. It requires Follow + edge stops;
it does not silently select a camera mode. Its portable preference is
`battle_edge_cover = 0/1` in `Settings/battle-camera.ini`. Old files default Off,
and old camera/compatibility keys keep their meanings. Saves are atomic and
preserve unrelated keys; malformed settings are rejected without replacement.

Scope: exact-ROM-authenticated rocky arena 3, ordinary eligible Follow frames,
width 284 or 384, and the full combat compositor. Other arenas, field maps,
native size, Current, Bounded, legacy replay and rejected schedules retain their
existing behavior. HUD bands are excluded. Forced blank keeps its usual path.

This is a presentation mask, not recovered terrain or a measured opacity edge.
The trial reserves 32 columns at each end of the 384-column authored source,
projecting the opening through the completed upper-band horizontal scroll:

```
cover_begin = clamp(anchor + 32  - Hflat, original_begin, anchor)
cover_end   = clamp(anchor + 352 - Hflat, anchor + 240, original_end)
```

The 32-column inset is an aesthetic trial value, NOT a Ghidra-discovered pillar
boundary. The clamps always preserve the original 240-column GBA view at its
accepted Follow position. Only extra scenery may be covered. The native row
perspective, object submission, signed effect/window coordinates, HUD position,
camera anchor, actor/collision data and game simulation are unchanged. No ROM
or generated code is edited. The same owned horizontal key yields the same
opening regardless of jump height; cover bounds never feed back into Follow.

The existing matte/trim/shadow border replaces all composited layers outside
that opening, avoiding a BG0-only cut that would expose the distant backdrop.
At width 384 and the central Hflat=72/anchor=72 state, the opening changes from
346 to 320 columns with no zoom. Near the ends it is asymmetric and may widen
to protect the original view. If a pillar cutoff is already inside the original
GBA view, this conservative cover deliberately will not hide it; a stronger
crop would require a separately reviewed contract and visual approval.

The Swordcraft workflow kept scene ownership/source coordinates separate from
art opacity and camera/collision limits. Assertions use register schedules,
framing geometry, composition counters and guest state, never screenshots or
framebuffer comparisons. The result still needs the user's aesthetic review.

Source: `src/battle_scenery_edges.h`, `src/custom_battle_scene.h`,
`src/custom_renderer.cpp`, `src/custom_renderer.h`, `src/main.cpp`,
`src/battle_camera_preferences.h`; four affected camera/compositor/preferences
tests. `tools/validate_battle_edge_cover.py` adds isolated paired runs and uses
read-only original captures. Existing session/matrix helpers accept an optional
edge-cover setting without changing their defaults. Installation remains
limited to the private Camera Edge Test through the proof-gated updater.

Verification and private installation:

- All 27 rebuilt C++ regressions and 15 Python fixture-safety checks passed.
  The actual Japanese/English 1.0.6.f ROM guards also passed, including deliberate
  corruption rejection. Geometry tests cover 3,404 admitted source-bound cases,
  jump-invariant covers, unchanged anchors, native-view containment, other-arena
  exclusions and composition dispatch counts without inspecting output pixels.
- `validation/battle-edge-cover-1790887499532142000/REPORT.json` passed twelve
  paired scenarios, a paired 934-observation replay of the user's capture, and a
  paired rocky battle/result/field-return route: 19,326 recorded observations in
  total. Every compared guest memory/cycle hash and camera-anchor record matched.
  Cover engagement was measured, not inferred from an unchanged-state result.
- The first capture route in `validation/battle-edge-cover-1790886918308373500`
  only reached native camera 101..128 because its leftward walking segment was
  too short for that saved battle. Its failed report remains intact. The second
  run rechecked all twelve completed comparisons against identical protected
  binaries/source/inputs, then reran the capture with 720 leftward input frames.
  Both camera endpoints were reached naturally; no actor/HP/collision data was
  changed. Its 933 eligible captured frames exercised the cover with 96 stable
  horizontal keys. The separate positive exit route retired combat ownership on
  results/field return and kept matching guest state.
- `validation/camera-cover-update-20261001-204808-fc57/REPORT.json` passed actual
  packaged launcher switch On/Off/save, English PLAY, both language boot and
  Escape/manual/automatic-pause/cancel checks, and running/paused F10 exports.
  Only the English/Japanese Camera Edge Test engines were replaced. Its settings,
  saves and every other file, plus the entire main Portable Beta, are hash-verified
  unchanged. Verified prior engines are in that report folder's `rollback/`.
- The installed setting remains Off until the player enables **Cover scenery
  edges** in launcher Mods or Esc > Graphics, with **Follow + edge stops** selected.
  No archive, commit, push or main-portable promotion was performed.

This certifies the measured state/geometry contract, not the final appearance,
all spells/bosses or long-play performance. Exact opaque pillar extents remain
unverified; visual approval of this conservative mask is still needed.
