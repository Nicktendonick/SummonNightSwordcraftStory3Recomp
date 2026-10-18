# Portable candidate verification

Status: local candidate under test, **not approved for publication**.
This record separates evidence about one binary from checks still needed.

## Tested identity

October 17 candidate `20261017T031439Z-a60c6fd3` was built from game revision
`400942bc39fa9e49ca35897c02e84972b12f8b3a`, engine
`3a7e974759b2a79ad4144396d571da6f004ba393`, and UI
`0aae3c6cfbbf10ba5c4e37165e13a116fa3a820f`.
It contains independently generated Japanese and English 1.1.0f engines.
The ZIP SHA-256 is
`3707ecd2756ba992019d74cd43b9f3ae214621249d45b932760c0827daf29f17`.
Later documentation/test-tool revisions do not change this binary's identity.
Rebuilding or changing packaged content requires a new manifest and candidate.

## Evidence already obtained

- Fresh recursive public clone, pinned dependencies and fresh playable build,
  without borrowing old guest object files.
- Allowlisted ZIP: 53 payload files, required DLL closure, exact input/build
  records, and unchanged PE loaded code after stripping debug data from copies.
  ROM/BIOS/patch/save/settings/capture/log payloads are excluded.
- Same-PC first launch with a system-only PATH; empty-input Play blocked, no
  developer input discovery, normal close. This is not a clean-machine test.
- State-based 12:5 arena smoke routes for all 17 normal IDs, in all four framing
  modes. Repeat mode also checked the four duplicate/native-fallback slots.
  Paired Current/Repeat routes reached both camera limits for IDs 0, 4 and 15
  with equal guest-state hashes. In Repeat, native auto-battle completed and
  returned to authenticated field ownership for all 17 arena fixtures, without
  HP/enemy/AI memory edits. Asset selection fixtures share one encounter;
  this does not cover every boss, spell or story route, or certify scenery joins.
- Both engines cold-booted 600 frames with zero dispatch misses, followed by
  exact state restoration and deterministic 120-frame replay.
- Scripted Esc dismissal, automatic/manual pause, Resume, reset/close
  cancellation, confirmed close/reset, and F10 export gates/repetition/restore.
  These use dummy output/test hooks, not physical keyboard or audible proof.
- Guard matches native player state/timing; six R slots remain selected;
  airborne/pause/release and restore behavior pass. Mod Off restores native
  auto-battle. Ordinary inputs and noncombat Select have equal on/off guest
  hashes. Combat fixtures are English; Japanese has separate noncombat boot
  comparisons, not an English snapshot relabelled as Japanese.
- Real desktop launcher, driven through scripted model/menu APIs: original JP,
  JP plus the 1.1.0f BPS, switching back to JP, and prepatched English launch the
  expected engine/save bank. Imported ROM basename is retained. A malformed
  tiny ROM and a same-size synthetic unsupported ROM are blocked; this is not
  exhaustive unsupported-version coverage.
- All five presets reach the runtime correctly. A custom filter/effect/colour
  selection and all three aspects survive runtime-to-launcher reopening.
  Width changes wait for Resume as described in the UI. Seven launcher pages
  retain their organization, including English Translation under Mods.

Private reports live under the owning project's ignored
`validation/release-readiness-20261016`, `validation/ui-a60c6fd3-v3`, and the
readiness checkout's `validation/guard-candidate110`. They contain local paths
and may reference game-derived states; do not upload the directories publicly.
No screenshot/pixel assertions were used for these runtime claims.

The expanded `validation/ui-a60c6fd3-v4` run repeated the model/settings checks
and added same-size invalid-input refusal. Its final keyboard case failed:
the old scripted-key helper omitted the SDL window ID, and ImGui rejects those
events. This does not prove a physical-key defect. A separate UI branch fixes
the test helper; keyboard acceptance must be rerun on a rebuilt candidate.
Do not treat that partial report as an entirely passing run.

## Open release gates

- Complete controlled performance/real-audio measurements and additional
  transition coverage where justified; investigate demonstrated bottlenecks,
  not guesses. The 17-fixture exit route is not every story/boss transition.
- Verify keyboard-only navigation and physical controller reconnect, including
  disconnect while holding an input. Merely detecting a controller is not proof.
- Human visual acceptance of field/battle joins, camera edges, UI layout,
  spells/effects, and audible acceptance. Do not turn automated state checks
  into claims of visual quality or original-hardware equivalence.
- Weaker-hardware measurement with CPU/GPU/OS, settings, scene, timing and audio
  counters recorded. The developer PC cannot substitute for that hardware.
- Clean-machine extraction/launch without compiler tooling, separate JP/English
  setup, persistence after moving the complete folder, save import and input
  hardware checks. Do not distribute private test inputs with the candidate.
- Reconcile preserved local experiments, refresh final candidate for the
  approved source revision, and run hosted ROM-free CI after approved push.
- Resolve distribution/provenance review and obtain explicit approval before
  publishing, pushing/merging changes, or contacting external testers.

For external testing, record the exact ZIP hash, test machine and each outcome
as pass/fail/not tested, with reproduction steps. Keep the old installation and
saves backed up. Unavailable gates remain open unless the owner explicitly
changes the release scope; a green unit test run does not waive them.
