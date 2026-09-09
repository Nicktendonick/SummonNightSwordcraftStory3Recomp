# Alpha unfinished-build v.01

**Local review candidate prepared. Not uploaded, published, or cleared for distribution.**

[Open the ZIP](../release-stage/alpha-unfinished-build-v.01/20260908T001923Z-1e08648b/SummonNightSwordcraftStory3Recomp-alpha-unfinished-build-v.01-Windows-x64-LOCAL-REVIEW.zip)

[File list with sizes and SHA-256](../release-stage/alpha-unfinished-build-v.01/20260908T001923Z-1e08648b/FILE-LIST.txt)
| [Technical review report](../release-stage/alpha-unfinished-build-v.01/20260908T001923Z-1e08648b/LOCAL-REVIEW-REPORT.json)
| [Draft release notes](../packaging/alpha-v01/RELEASE-NOTES.md)
| [Open distribution review](../packaging/alpha-v01/LICENSE-REVIEW.md)

- Variant: existing English-translation-beta executable; no gameplay changes.
- Archive: 20,027,776 bytes (about 20 MB), 33 files.
- SHA-256: `0add8fe8afe1542e7d5c93174225c9aa2b86e4ce7cbb2dcaaeaf35b9a3b88d2f`.
- Contents: one executable, four DLLs, ten launcher assets, five instructions /
  launch files, eleven available license notices, manifest and checksum list.
- Working executable and existing saves remain untouched. Submodules unchanged.
- No separate ROM, BIOS file, BPS/IPS patch, generated source/object, save,
  recording, capture, developer configuration or compiler is in the ZIP.

## Verified locally

- 10 packaging unit tests passed (including unexpected-file, traversal,
  checksum-tampering, case-collision and missing-DLL rejection).
- Three existing focused CTest checks passed: widescreen route auditor,
  runtime monolith guard and PPU smoke tests.
- Copied executable reduced from 229,686,304 to 44,023,424 bytes by removing
  debug information. Loaded code/data, import names and entry point verified
  unchanged for the executable and all four DLLs.
- Static/delay PE dependency closure and exact ZIP manifest/checksums passed.
- Extracted `--help` and invalid-argument rejection passed with only Windows
  system directories on PATH. No development DLL/tool lookup was needed for
  those checks. Common developer user/toolchain path markers were not detected
  in the stripped binaries; this is not an exhaustive privacy/code audit.

An earlier automated missing-input test waited on the runtime's Windows file
picker and timed out; it was not a game crash. Missing-input selection is now
explicitly a manual test, not falsely counted as a passed automated rejection.
The earlier candidate is retained locally for inspection; use the ZIP linked
above, whose corrected automated checks passed.

## Before GitHub publication

1. Complete the compiled game/translation/BIOS distribution review and the
   remaining root/dependency/font/asset license/provenance work. No rights have
   been assumed or license chosen for the owner's source.
2. Run the [clean-machine interactive checklist](../packaging/alpha-v01/SMOKE-TEST.md)
   against this exact archive. Startup smoke checks do not certify playability.
3. Review the final artifact list with the owner and obtain publication approval.

The proposed GitHub title is **Alpha unfinished-build v.01**, with tag
`alpha-unfinished-build-v.01` and prerelease enabled. No tag/release or visibility
change has been made. After the gates pass, use GitHub's **Draft a new release**
flow, select the reviewed commit, use those title/tag settings, attach only the
approved final archive/checksums, and keep the existing private visibility.
Do not upload the raw build or used test directory.

## Make another local candidate

Double-click `Prepare Alpha Release.bat` at the project root. It requires the
development machine's Python 3.11+ and MinGW `strip.exe`, but the resulting game
package does not need Python to launch. Every run creates a fresh timestamped
directory under `release-stage/alpha-unfinished-build-v.01`; earlier candidates
are preserved. The recipe intentionally pins this working executable and the
game/submodule commits. Review the recipe before packaging a different build.

All durable files stay inside this project's Documents folder. The `release-stage`
directory is Git-ignored; notes, packaging recipe, tests and notices are local
source changes ready for a later reviewed commit, not yet pushed.
