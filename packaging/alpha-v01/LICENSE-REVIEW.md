# Distribution review -- OPEN

This local package is not cleared for upload, including to a private release.
The file allowlist and hash checks are technical checks, not a legal opinion
or a grant of rights. Do not infer permission from this document.

## Required decisions before distribution

1. Review the compiled game, translation-derived and BIOS-derived code in the
   executable. It was linked from generated source; requiring external input
   files does not settle whether this binary may be distributed.
2. Resolve the root game repository's intended license and contributor
   provenance. No license has been selected on the owner's behalf.
3. Finish the full linked-code and asset inventory, including applicable source
   availability and dependency conditions. Copied notices below are only an
   inventory of available local records, not an assertion of completeness.
4. Confirm that all applicable obligations are met for the exact final artifact,
   then obtain the owner's approval of its contents and publication.

| Component | Available evidence / outstanding work |
| --- | --- |
| gbarecomp | Pinned LICENSE: PolyForm Noncommercial 1.0.0 plus licensor clarification. Full text included. |
| JRickey ports / mGBA-derived portions | gbarecomp THIRD_PARTY_ATTRIBUTION.md records MIT OR Apache-2.0 and MPL-2.0 portions. Full applicable notices/source arrangements still need review. |
| recomp-ui | Pinned MIT LICENSE included. |
| Dear ImGui | Pinned MIT LICENSE included. |
| toml++ | Pinned v3.4.0 MIT LICENSE included; verify final linked use. |
| SDL2 | Installed package 2.30.11-1 license included; binary hashes compared with that installation. |
| libgcc / libstdc++ | Installed gcc-libs 14.2.0-2 GPL-3.0 and GCC Runtime Library Exception 3.1 texts/README included. Check distribution/source obligations, not just executable linking. |
| libwinpthread | Installed package 12.0.0.r473.gce0d0bfb7-1 COPYING included. |
| LatoLatin, OpenMoji, Noto Symbols fonts | Upstream two-font NOTICE included, but complete matching font licenses/provenance still need collecting. |
| Other embedded UI code and images | Review stb, tinyfiledialogs, generated GL loader and each of the six launcher images; do not assume the root UI license covers everything. |

No raw game or BIOS image, patch, generated source/object, personal config or
capture was allowlisted. That exclusion does not remove translated code or
necessarily all source-path strings from compiled binaries. The packager
removes debug sections from copies only and verifies the retained loaded
sections are unchanged. See its local report for path-string scan findings.

If the compiled executable cannot be cleared for distribution, review a
user-local generation/build delivery approach separately. Do not silently
substitute a different architecture or call this candidate cleared.
