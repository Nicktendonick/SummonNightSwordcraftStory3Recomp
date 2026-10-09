# Portable UI release dry run

Prepared October 3, 2026 (America/New_York), for a tester, not a public release.

Archive: `release/Swordcraft-Story-3-Portable-Beta-UI-Dry-Run-2026-10-03-Windows-x64.zip` under the owner project.
SHA-256: `66532a84216930f07676f8d9bc29639575949ba550a4c65262aa6b106fe2227d`.
Size: 76,091,549 bytes. Payload: 53 files plus empty player-data directories.

## Contents and checks

- Uses the exact English 1.0.6.f and Japanese source engines authenticated by
  `validation/ui-cleanup-20261004-032911-ca29/report.json`.
- Organized launcher and Esc menu; English Translation remains in Mods.
- Top-level `Swordcraft Story 3 Beta.exe`, Runtime, Credits and portable folders.
- No ROM, BIOS, translation patch, saves, user settings, captures or logs.
- Updated README matches the new UI and tells testers what inputs to supply.
- Existing artwork/dependency notices and open public-distribution review retained.
- Exact allowlist, duplicate/path safety, payload manifest and checksums verified.
- Debug stripping preserves loaded sections, imports and entry point.
- Packaged DLL dependency audit passed.
- Fresh extraction passed read-only starter preflight and empty-input startup
  with a system-only PATH and different working directory. It quit normally,
  did not discover developer ROM/BIOS/saves, and left original payload unchanged.
- Earlier UI checks passed both language launch paths and the 58-test suite.
  This packaging turn did not repeat a full gameplay playtest or test a second PC.
- No installed portable was replaced and no GitHub upload was performed.

## Reproduce

From the custom-renderer worktree, run `tools/package_portable_release.py`.
The packager uses a fixed program-file allowlist, not a played portable folder.
Run `tools/test_portable_release_first_run.ps1` with the resulting archive,
a new destination under validation, and the configured Python runtime.

The candidate and machine-readable package report are under owner
`release/Portable Release Candidates/20261004T034540Z-3b7f15a9`.
The clean-start test extraction is
`validation/ui-release-first-run-3b7f15a9`.
The shorter disposable extraction is `validation/pkg-3b7f15a9`.
Test extractions may create local settings/logs; never distribute them.

The intended next check is a tester's first run on another PC: import their
own inputs, launch Japanese or English, inspect Graphics and Mods, and test
Esc pause/resume with their keyboard/controller. Public-distribution review
and broader gameplay coverage remain open.
