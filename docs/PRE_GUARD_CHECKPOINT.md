# Portable beta checkpoint before the Guard experiment

This source checkpoint preserves the completed portable-beta work before any
dedicated Guard / Select-remapping gameplay experiment. No Guard mod or change
to the game's Start, Select, auto-battle or blocking behaviour is included.

## Included work

- Original Japanese ROM support without a translation; optional English patch
  support, revision-matched engines and separate language save banks.
- Readable imported filenames with collision protection and honest labels for
  verified legacy numeric imports.
- Portable release-candidate packaging with a fixed payload allowlist, empty
  player-data folders, manifests and archive-safety checks.
- Nearest, Linear, Sharp fractional and Smooth 2x scaling; optional LCD Grid and
  CRT effects, shared launcher/Esc-menu preferences and reset persistence.
- Original GBA (3:2), Widescreen (16:9) and Ultrawide (12:5) host presentation
  choices in both menus, applied at a completed-frame boundary.

The game adapter, reusable runtime and reusable UI remain in their respective
repositories. The parent repository pins the corresponding submodule commits.
ROMs, BIOS, patches, saves, generated game code, private captures and compiled
release packages are excluded from this source checkpoint.

## Checkpoint verification

Immediately before publication, the seven targeted CTest entries passed:
presentation preferences, presentation filters, portable language, runtime menu,
portable launcher, portable input and beta artwork selection. All five ROM-free
portable archive-safety tests also passed. Git whitespace checks and a changed-file
private-input/binary/credential-pattern scan were clear across all three repos.

These are targeted checkpoint checks using the existing built test programs,
not a new full build or a full-game certification. Earlier integration evidence
and its limitations are recorded in:

- [Japanese and optional English support](PORTABLE_LANGUAGES_20260928.md)
- [Readable imports and portable packaging](PORTABLE_RELEASE_20260928.md)
- [Presentation filters and combat timing](PRESENTATION_FILTERS_20260928.md)
- [Live aspect switching and installed package](HOST_ASPECTS_20260928.md)

Statements in those dated reports that no commit/push occurred describe their
original implementation tasks, before this source-publication checkpoint.

## Release boundary

This checkpoint does not replace release downloads, move release tags, change
repository visibility or overwrite the installed Portable Beta. The existing
[distribution review](../packaging/portable-beta/RELEASE-REVIEW.md) remains open.
The portable candidate and player installation stay local and unchanged.
