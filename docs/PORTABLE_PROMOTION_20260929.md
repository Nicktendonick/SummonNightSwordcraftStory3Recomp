# Main Portable Beta promotion — 2026-09-29

**Subsequent update:** [Guard mod release](GUARD_MOD_RELEASE_20260929.md) adds
the saved Mods toggle to the normal entry point and retires the separate Guard
starter. The records below describe the earlier promotion.

Completed at the user's request. This supersedes the earlier audit's statement
that the main install still has the old Smooth engines; it does not close the
audit's remaining renderer, Guard UI, coverage or public-distribution items.

## Installed result

Owner-relative target: `release/Portable Beta`.

- `Swordcraft Story 3 Beta.exe`: normal Select/auto-battle behavior, optimized
  Smooth 2x available. This remains the main entry point.
- `Swordcraft Story 3 Guard Test.exe`: optional combat-only Select hold/release
  Guard experiment, using the same portable settings and language save banks.
  Close the game before switching starters. No Mods checkbox was added.
- Both English/Japanese engines, four matching runtime DLLs and fourteen
  dependency/artwork/review notice files were promoted from the tested private
  package. The main README now explains the experiment and current performance.

Eight existing files were replaced: the normal starter, two engines, four DLLs,
and README. Fifteen were added: the Guard starter and fourteen
notices. All **105 other existing files** were hash-checked unchanged, including
ROMs, BIOS, patches, battery saves, save states, settings, credits, artwork and
logs. The private Guard source folder was also hash-checked unchanged. No user
process was killed and no player save/config was used as a test output.

No game source behavior changed in this promotion. A build freshness check
rebuilt the normal starter; both engine builds and the Guard starter needed no
rebuild. The promoted, already-tested stripped EXEs/DLLs match their current
builds' loaded PE sections, imports and entry points. Normal starter recompilation
did not change its loaded code/data relative to the tested stripped copy.
Dependencies resolve from the portable Runtime folder.

Engine SHA-256:

- English: `324057340aaa3667cb22118b65a63f720c1ba318d72c57a61417af0a12d1f4ee`
- Japanese: `c72c0c7afd6c2e4a56760ae918a9f21ffce6df055c93a67362dc587ab27148bf`

## Verification after installation

- Both installed starters passed their read-only `--check` preflight.
- The exact installed engines passed the Guard regression suite: native B vs
  Select player-state parity, six ability slots, hold/release, airborne/native
  pause gates, ordinary inputs, field Select noninterference, AI cancellation,
  save-state restoration and Japanese cold-boot/noncombat equality. Japanese
  combat is still not certified.
- Copies of both starters opened and closed normally in a separate empty-input
  folder with PATH reduced to Windows directories. The native launcher model
  visited settings/controls/assist/mods/credits, accepted Smooth/LCD settings,
  cycled all three aspects, and kept Play disabled without private game inputs.
  No ROM, BIOS or player save was borrowed. No screenshot assertions were used.
- The actual installed English engine passed the isolated Esc-menu route:
  automatic/manual pause, Resume, running behind the menu, 240/284/384 aspect
  application, Smooth off/on, and private slot-9 save/restore. There were zero
  presentation filter fallbacks. These paused-menu checks are not FPS results.
- A final whole-folder hash check confirmed exactly the intended update, with
  all other player files and the private source package unchanged.

The initial staging attempt stopped before touching the install because the
older main DLL files differed from the tested DLLs. The final explicit allowlist
includes the tested dependency set and notices. An initial smoke harness attempt
stopped before launching because of environment-key casing (`SYSTEMROOT`);
correcting the harness and rerunning completed successfully. Neither attempt
was counted as passing game validation.

## Rollback and evidence

Owner-relative rollback folder:
`release/Portable Beta Rollback 20260929-154300-6c92`.

It contains verified copies of all eight replaced files and `UPDATE-REPORT.json`
with before/after hashes and the added-file list. No rollback was needed. Close
the game before any future rollback; restore only its recorded program/README
files, never overwrite player data. Added files can be moved aside recoverably.

Private evidence under this checkout (do not upload):

- `validation/portable-promotion-20260929-154135-4dda/INSTALLED.json`
- `validation/guard-1790696626924709800/report.json`
- `validation/prsm-dcc3c77d/report.json`

`tools/promote_portable_beta.py` implements staged, hash-pinned promotion with
backups and an explicit allowlist. Its provenance record includes changed and
untracked source-file hashes, not only Git diff. This is a record for this local
promotion; it does not fix the separate public ZIP packager's provenance gap.
`tools/verify_portable_promotion.py` performs the isolated smoke checks.

## Unchanged limits

Mine Select/event framing, arena 8 native fallback, production Guard UI,
Japanese combat coverage and clean-machine/perceptual audio testing remain
open. No Git commits, pushes, tags, public ZIPs or GitHub releases were changed.
This played local folder contains private game data and must not be uploaded.
