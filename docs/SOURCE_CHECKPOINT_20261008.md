# Pre-cleanup source checkpoint — October 8, 2026

This checkpoint preserves the active `experiments/full-viewport-renderer`
source before the requested project cleanup. It is not a release, a replacement
portable ZIP, or certification that a fresh public clone builds successfully.

## Branches and dependency pins

- `checkpoint/pre-cleanup-20261008`: current UI/camera/translation work without
  the SDL2 2.32.10 experiment. Its engine and launcher submodule pins identify
  the corresponding source snapshots in their own repositories.
- `checkpoint/sdl2-test-20261008`: the same work plus the separately tested
  SDL2 SDK pin, staging and packaging changes. Its engine pin also includes
  the optional SDK DLL staging change; the launcher pin is unchanged.

The existing development branches, default branches and portable distributions
are not replaced. Older alternate worktrees remain local and are not covered
by this active-worktree checkpoint. No cleanup or file retirement is performed.

## Preserved work

Organized launcher/Esc settings, the corrected three-line Craftknights subtitle,
Escape-to-resume, normal-launcher F10 capture, Japanese and English 1.0.6.f
engine selection, optional battle framing/scenery-edge coverage, Guard patch
source, and their regression/packaging tools and investigation notes.

ROMs, BIOS, translation patches, generated game code, saves, private test
fixtures, captures, local SDKs and compiled programs are excluded. Diagnostic
tools require private inputs that are deliberately not included.

## Known build and release blockers

The `gbarecomp` submodule points to the private
`Nicktendonick/gbarecomp-swordcraft3` repository. Unauthorized testers receive
"Repository not found" before compilation. A checkpoint does not fix access;
granting access or publishing a reviewed engine is a separate owner decision.
Repository visibility is unchanged.

Some build/packaging helpers still assume the owner's nested checkout layout,
existing generated BIOS/guest-object caches and local tool paths. Removing or
archiving those inputs must wait for the reproducible-build cleanup and a
fresh-checkout verification. Do not retire old build folders solely because
they look unused.

The installed `release/Portable UI Test` is unchanged. `Portable SDL2 Test`
is a separate private test copy; SDL2 performance approval remains open.
The October 3 tester ZIP is also unchanged. Neither played portable folder
should be redistributed with its private inputs.

## Verification scope

Earlier behavior evidence is recorded in [UI cleanup](UI_CLEANUP_TEST.md),
[camera work](FOLLOW_EDGE_CAMERA.md), and [portable dry run](PORTABLE_UI_DRY_RUN.md).
Checkpoint integrity checks are separate from a clean rebuild. No claim is
made that all arenas, physical controllers, or full-game behavior are certified.

The checkpoint pass reran the existing 58 configured regression binaries and
23 ROM-free packaging/arena/input-audit tests successfully. Its initial test
invocation omitted the MinGW runtime folder from the process search path,
causing DLL-loader errors before some tests could start. Adding the existing
build and compiler DLL folders to that process's PATH resolved those startup
errors; no system installation or portable-file changes were needed.
