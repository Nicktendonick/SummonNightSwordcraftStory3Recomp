# F10 capture — installed in Camera Edge Test only

October 1, 2026. The owner initially deferred portable preparation, then
explicitly requested the F10 fix in the existing Camera Edge Test so captures
could be collected. Main Portable Beta and camera behavior remain unchanged.

## Cause and fix

The SDL key handler and runtime call site accepted a launcher-provided
`GBARECOMP_DEBUG_CAPTURE_DIR`, but the exporter itself required
`GBARECOMP_VISIBLE_DEBUGGER`. Normal portable play therefore silently returned
without saving anything.

The runtime and SDL handler now share `debug_capture_enabled`: either visible
debugger mode or a nonempty capture directory enables F10. Empty/unset paths
without debugger mode do not enable capture. Debugger-only F2–F9 behavior stays
separate; normal F1–F9 load and Shift+F1–F9 save remain intact. Key-repeat is
still ignored. The export remains `frame.png`, `state.gbas`, and `report.json`
under the configured session folder; it is diagnostic output, not a replacement
for the player's normal save slots.

`GBARECOMP_ASSIST_SCRIPT` accepts `capture` to exercise the same guarded export
without desktop input. This is a test facility, not a new user-facing control.

## Verification

- `swordcraft3_portable_input_tests` passed: real in-process SDL F10 events,
  disabled/debugger/portable gates, ignored repeat events, all nine ordinary
  save/load keys, debugger isolation, and existing input/menu/presentation tests.
- `tools/validate_f10_capture.py` passed eight isolated cases: baseline,
  disabled export, portable path with spaces, explicit debugger-off,
  debugger fallback directory, unwritable destination, pause/resume capture,
  and reloading an exported state. Enabled cases generated two separate
  complete capture bundles. Per-frame guest state matched the no-capture run.
- Every clean-boot test used strict static execution with zero dispatch misses.
  No screenshot, pixel, or framebuffer comparisons were used as assertions.
- Latest evidence: `validation/f10-capture-20261001-050928-c72d/REPORT.json`.
  Pause coverage explicitly opens the menu and observes pause/resume status
  on the following input pump, after queued actions apply. This strengthens
  the original test, which issued menu actions while the menu was closed.
  The initial, preserved attempt used an older combat fixture and stopped at
  strict-static missing PC `0x03003240` before testing export. That unrelated
  fixture/code-coverage issue was not fixed or bypassed; the passing tests use
  clean boot instead. This work does not claim a combat-camera validation.

## Test-copy installation

`tools/update_camera_test_f10.py` rebuilt/staged only the existing English
1.0.6 and Japanese engines. Before installation, a disposable copy passed the
actual English launcher PLAY flow, running and paused exports, Resume and
confirmed Close. The Japanese engine also exported two complete bundles.
Debugger mode was disabled and both runs reported zero dispatch misses with
strict-static execution. SDL F10 routing and all nine save/load keys passed
the separate real-event input regression again.

Only these files in the owner's `release/Portable Camera Edge Test` changed:

- `Runtime/Swordcraft3CustomRendererBeta.exe`
- `Runtime/Swordcraft3Japanese.exe`

The launcher, settings, saves, save states, ROMs, patches, assets and all other
test-folder files were verified unchanged. Main Portable Beta was verified
byte-for-byte unchanged. The installed launcher passed its read-only preflight.
Run the same `Swordcraft Story 3 Beta.exe`; F10 writes capture bundles beneath
that copy's `Captures/<session>/frame-...` folder.

Install proof and verified rollback engines:
`validation/camera-f10-update-20261001-051005-3478/REPORT.json` and its
`rollback/` directory. Failed disposable smoke attempts remain as evidence;
none installed anything. No commit, push, publication or camera change occurred.

## Old portable archive

At the owner's request, 18 old build/rollback folders and three old ZIP/7z
archives moved from the owner's `release` directory to the sibling
`OldSCS3Portables` directory. Current `Portable Beta` and
`Portable Camera Edge Test` remain in place. No files were deleted or replaced;
all moved file bytes and directory entries were verified before/after.

The relocation record is at the owner root:
`validation/old-portable-archive-20261001-044443-8322/MANIFEST.json`.
It lists exact original/destination paths and per-file hashes for restoration.
Historical validation records retain their original paths as provenance; the
four tools with a fixed Guard Experiment input now reference its archived path.
The old in-place Guard updater intentionally still requires a live copy under
`release`, rather than mutating the archived rollback collection.
