# Portable release candidate and readable imports — 2026-09-28

## Requested player distribution

The main player download should be a portable Windows x64 ZIP with the starter
EXE beside Runtime, Credits and the eight writable data folders. Automatic
GitHub source ZIPs are not runnable distributions.

`tools/package_portable_release.py` creates a NEW local candidate under the
owner project's `release/Portable Release Candidates`. It never archives or
overwrites a played installation and has no upload option. Its fixed allowlist
contains 52 files including program/DLLs, the eleven covers plus logo/header,
fonts, credits, notices, README, manifest and checksums. Data folders start empty.
It verifies source/build asset agreement, x64 PE import closure, and unchanged
loaded sections after removing debug sections from copies only. It extracts the
exact ZIP into a separate validation directory for testing.

The old user-created `release/Portable Beta.zip` contains private inputs/state
and was NOT reused, modified or uploaded. The active player install was not
used as the packaging source.

## Readable filenames

- Engine `portable_files.h` preserves the basename of newly imported ROM, BIOS
  and patch files. Different bytes under the same name get `name (2).ext`, etc.
  Identical content at that name is reused; originals are never modified.
- Relative selection paths still provide portability. Filenames need not be hashes.
- Existing numeric ROM copies are not renamed or removed. The UI recognizes the
  former decimal naming scheme against the entire file's FNV fingerprint, THEN
  requires the existing ROM identity verification before substituting a label.
  The label shows the game and source/patched status, not an invented filename.
- An ordinary numeric filename or unverified ROM is not relabeled by guesswork.
- Change ROM can reimport the original file to restore the original filename.
  That name cannot be recovered from old imports because it was never recorded.
- Filename display accommodates the full supported path-buffer basename and
  wraps in the Home panel. The patcher's internal output cache is unchanged.

## Completed checks

- Rebuilt runtime and launcher/input/menu regression targets.
- All 28 selected CTest cases passed, including readable import names, duplicate
  safety, stale import temp protection, reopening, legacy source/patched labels,
  unverified/numeric-name negative cases, and existing rendering-state tests.
- Five ROM-free ZIP-verifier tests passed: clean layout accepted; payload changes,
  unexpected BIOS, personal settings and a missing empty data folder rejected.
- Exact extracted package: keyboard remap persisted on close and after relocation;
  startup worked with a system-only PATH and unrelated working directory.
- All six native launcher pages, three window sizes, eleven artwork rotations,
  credits/tool navigation and input persistence passed state/log-based checks.
- Actual runtime pause/resume, Cancel-first Reset/Close, clean-process reset and
  persisted menu-pause preference passed in both presentation modes. Test battery
  hashes were unchanged. Inputs/state were test-owned copies.
- Separate input-free extraction: read-only preflight, first launcher opening,
  normal close, no developer ROM/BIOS/save discovery, original payload unchanged.
- No screenshot or pixel comparisons were used as assertions.

This is bounded developer-PC evidence, not a full playthrough or a separate
clean-machine certification. Existing chest/rewind issues are not claimed fixed.

## Local artifact and deployment

Candidate directory: `release/Portable Release Candidates/20260928T054135Z-47e6a352`

ZIP: `Swordcraft-Story-3-Portable-Beta-Windows-x64-CANDIDATE.zip`

- ZIP bytes: 58,596,753
- ZIP SHA-256: `588db8035885ccd3f75442dd8d20f511fe8ec58422735826c54b6065ba3edcd6`
- Packaged/installed runtime SHA-256:
  `26f1695a4303caedbae4be307c9badf25d3b2ffd6261197464f654fd8455715d`
- Unstripped build runtime SHA-256:
  `25a528da3c735691003418c8e337d1da381360e2cfcba0a30155a1904194fa34`

Only the runtime EXE in the owner's existing `release/Portable Beta` was updated.
All 15 existing files across Settings/Saves/Save States/ROMs/BIOS/Mods/Credits were
hash-checked unchanged. The previous runtime is retained at
`release/Filename Rollback 20260928/Swordcraft3CustomRendererBeta.exe`.
Close the game before restoring that file if rollback is needed.

## Publication status

No commit, push, release upload, tag movement or repository visibility change
was made. The filename fix is local and newer than prerelease06. The manifest
records the prior source commits plus dirty-state/diff identities, not a claim
that these local changes are already on GitHub.

The existing binary/artwork/complete-notice distribution review remains OPEN;
see `packaging/portable-beta/RELEASE-REVIEW.md`. A clean archive is not evidence
that those separate questions are resolved. After review and source publication,
attach the portable ZIP/checksum and make its direct download the first player
link in release notes and README. Keep Source code as a developer option.
