# Prerelease preparation

Recorded 2026-09-07. This document captures the owner's requirements and an
initial inspection of the current build. It is not a completed distribution
audit, an approved package manifest, or a published release.

The development checkpoint is `capstone-pre-astra-2026-09-07` at `d2cbeaa`.
The owner-selected prerelease title is **Alpha unfinished-build v.01**.
The reserved tag spelling is `alpha-unfinished-build-v.01` (Git tags cannot
contain spaces). Neither a tag nor a GitHub release has been created by the
local packaging workflow. The current build is the English-translation-beta
variant; "Alpha" describes this PC port, not a new translation patch version.

Run `Prepare Alpha Release.bat` to produce a fresh, local-only review candidate
under `release-stage/alpha-unfinished-build-v.01/`. See
`packaging/alpha-v01/RELEASE-NOTES.md` and `LICENSE-REVIEW.md` for its scope and
unresolved publication gates. This snapshots the existing game; it does not
implement the proposed tan battle HUD borders or change the working build.
The completed local candidate's inventory and actual test results are recorded
in [ALPHA_V01_STATUS.md](ALPHA_V01_STATUS.md).

## Required sequence

1. Complete the accepted implementation and owner play-testing. Record the
   tested game/submodule commits, build identity, supported ROM identities,
   and remaining limitations.
2. Audit the final executable, dependencies, assets, and licensing. Decide
   whether a compiled game executable is appropriate to distribute and verify
   the workflow using the player's own legally obtained ROM and BIOS.
3. Prepare a clean staging script, explicit artifact manifest, runtime DLLs and
   assets, required notices, SHA-256 checksums, smoke-test instructions, and
   release notes. Keep all staging below this project directory.
4. Test the exact staged archive on a clean machine/profile and present its
   artifact list, file sizes, checksums, provenance review, and test results to
   the owner.
5. Publish a clearly marked GitHub prerelease only after the owner's explicit
   request following that review. Repository visibility remains a separate
   owner decision. Do not attach a release action to an ordinary commit or tag
   push.

## Initial architecture findings

The current launcher and runtime accept external input files:

- `src/main.cpp` supplies ROM fingerprints and invokes the pre-boot launcher.
- `gbarecomp/src/runtime/launcher_seam.h` passes selected ROM/BIOS paths to the
  runtime. The beta additionally selects the user's translation patch and
  verifies its prepared output identity.
- `gbarecomp/src/runtime/runtime.cpp` loads the BIOS and reads and verifies the
  ROM from disk during startup.

However, those external files are not the only origin of code in the binary:

- Root `CMakeLists.txt` compiles locally generated game C/C++ into the Japanese
  executable and the separate translation-specific corpus into the beta.
- `gbarecomp/CMakeLists.txt` adds generated `bios_recompiled.cpp` and
  `bios_dispatch_table.cpp` to the runtime when present.
- The current `build-beta/build.ninja` confirms
  `GBARECOMP_HAVE_BIOS_RECOMP=1`.

Therefore, a file-picker requirement and the absence of a `.gba` or `.bin`
file from an archive do not establish whether the compiled executable is
appropriate to distribute. Review translated game/BIOS code, any embedded
data, translation-derived changes, debug information, source paths, and the
terms applying to each component. The existing development executable is not
approved for release by this document. If binary distribution cannot be
established, present a user-local generation/build workflow for review before
implementing a different delivery architecture.

The existing `gbarecomp/tools/package_release.ps1` targets `bios_smoke.exe` and
renames it `gba.exe`; it is not a packaging script for this game.

## Initial license inventory

These entries report files in the pinned checkouts, not a completed legal or
distribution determination. Review actual final linked/copied content and
obtain the applicable notices/source obligations for that release.

| Component | Evidence in the checkout | Follow-up |
|---|---|---|
| Game-owned source | No root LICENSE file found during the initial inventory | Resolve the intended license and contributor provenance; do not choose a license on the owner's behalf |
| gbarecomp | `gbarecomp/LICENSE`: PolyForm Noncommercial 1.0.0 plus licensor clarification | Review its noncommercial conditions and exact pinned text |
| Ported runtime portions | `gbarecomp/THIRD_PARTY_ATTRIBUTION.md`: MIT/Apache-2.0 and MPL-2.0 portions | Check actual linked code, full notices, and applicable source availability; do not rely solely on the attribution file's compliance assertions |
| recomp-ui | `recomp-ui/LICENSE`: MIT | Include the applicable notice |
| Dear ImGui | `recomp-ui/src/third_party/imgui/LICENSE.txt`: MIT | Include the applicable notice |
| OpenMoji / Noto Sans Symbols 2 | `recomp-ui/assets/common/fonts/NOTICE.md`: CC BY-SA 4.0 / OFL 1.1 | Obtain full applicable license text and review packaged font provenance |
| LatoLatin, image assets, other vendored libraries | Present in the asset/build manifests; not covered by the two-font notice alone | Locate license/attribution records for each shipped or embedded item |
| SDL2 / MinGW runtime DLLs | Imported by the inspected beta executable | Identify exact distributed versions, licenses, runtime exceptions, and applicable notice/source requirements |

## Package requirements

Use an explicit allowlist of reviewed files, never a recursive copy of a
development build directory. The staging script must reject unlisted files,
missing expected files, path traversal, and source/output paths outside the
intended project directories. Use a fresh staging destination for each
candidate and preserve previous candidates until deliberately removed.

The inspected beta executable directly imports four non-system DLLs:

- `SDL2.dll`
- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

This is a starting inventory only. Resolve transitive and dynamically loaded
dependencies for the final build, and verify which Windows components are
provided by the supported OS rather than copied from the development machine.

The current GBA asset staging manifest is
`recomp-ui/cmake/recomp_ui_assets.cmake`. Its output includes four fonts
(LatoLatin Regular/Bold, OpenMoji, Noto Sans Symbols 2), the common brand and
verdict images, and `pad_gba.tga`. Review each asset before selecting it for
the release manifest; do not add game art from a ROM or a captured frame.

The final manifest should record each package-relative path, originating
component/version/commit, purpose, license/provenance status, byte size, and
SHA-256. Supply a checksum list for the staged files and a separate checksum
for the final archive. Compute the archive checksum after the archive is
finalized. Include the exact game and submodule commits in release metadata.

Exclude the following from every uploaded artifact, including source bundles:

- Japanese ROM, GBA BIOS, translation BPS/IPS/IPS32 files, and prepared ROMs;
- saves, save states, recordings, screenshots, frame dumps, and debug captures;
- generated ROM/BIOS-derived sources, object files, static archives, and caches;
- personal `config*.ini`, `keybinds.ini`, `rom*.cfg`, `bios.cfg`, paths, or
  controller/account identifiers;
- development logs, symbol/debug files, compilation databases, and build/cache
  directories unless separately reviewed and expressly selected.

Create any needed default configuration from a reviewed template with no
personal paths. Scan nested archives and inspect the extracted final package,
not just its filename extensions. A private repository is not an exception to
these package rules.

## Clean-machine smoke test to finalize with the package

Use a Windows machine/VM or clean profile without the development repository,
MSYS2, Python, compiler tools, cached ROM paths, or development DLLs on PATH.

1. Extract the candidate archive into a directory containing spaces and start
   the launcher with no ROM, BIOS, or personal configuration present.
2. Confirm the first-run selection flow and useful missing/wrong-input errors.
   Then select the tester's own verified inputs; translation inputs, if
   supported, must also be tester-supplied and compatible with the corpus.
3. Check native boot, audio, keyboard/controller input, window resizing,
   supported widescreen modes. Tan battle HUD borders are not implemented in
   this checkpoint; do not describe an absent setting as tested.
4. Exercise a normal in-game save, save-state slots, rewind, fast-forward,
   return to normal speed, and a battle-to-overworld transition.
5. Restart and verify settings/saves are stored in the documented writable
   location. Check that no path reaches the developer's machine or repository.
6. Recheck the archive inventory/checksums and record the Windows version,
   graphics/audio devices, test coverage, failures, and unresolved issues.

Do not record these tests as passed until run against the exact candidate.

## Release notes and publication

The eventual notes should identify `Alpha unfinished-build v.01` as an experimental prerelease,
list supported operating systems and input identities, explain first-run
setup, describe included features and controls, and link the file manifest,
checksums, notices, source provenance, and bug-report instructions. State the
tested coverage and remaining limitations, including dynamic-IWRAM fallback
and save-state compatibility where still applicable.

A future manually triggered packaging workflow may build/audit a candidate and
produce review artifacts once executable distribution is resolved. It must
not automatically upload private inputs or publish a GitHub release. Final
release creation must be explicitly authorized after owner review, marked as
a prerelease, and must preserve the existing repository visibility.
