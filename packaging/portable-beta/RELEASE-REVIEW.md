# Portable beta distribution review — OPEN

The requested default player download is a Windows x64 portable ZIP with
`Swordcraft Story 3 Beta.exe` at the top level. Automatic GitHub source archives
are developer downloads, not playable packages.

This candidate is for local review. Before public upload:

1. Resolve distribution of compiled game-, translation- and BIOS-derived code.
   Excluding separate ROM/BIOS files does not settle that question.
2. Confirm distribution rights/provenance for the supplied artwork, logo and icon.
3. Finish applicable dependency/font notices and source-availability arrangements.
   Engine/UI forks are currently private; do not assume all obligations are met.
4. Test on a separate clean machine, including patching, controllers, audio and
   dynamic-code routes. Developer-PC tests with reduced PATH are not equivalent.

The fixed file allowlist, empty player-data folders, hashes, DLL inspection and
verified debug stripping are technical checks, not a legal opinion or permission.
No license or repository visibility is changed by preparing this candidate.

After review, attach the portable ZIP and checksum to a release, place its direct
download first in release notes/README, and leave Source code for developers.
Never upload an archive of a played installation: it may contain ROMs, BIOS,
patches, saves, personal settings or logs.
