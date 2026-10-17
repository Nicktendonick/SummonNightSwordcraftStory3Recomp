# Historical checkpoints and launch wrappers

These records explain how the port reached its current state. They are not
instructions for launching the current portable or claims about current builds.

## Root cleanup — October 16, 2026

- [Previous repository README, preserved verbatim](README-before-cleanup.md.txt)
- [Archived launch-wrapper sources](launchers/)

All 21 former root BAT wrappers are preserved here with `.txt` appended. They
cannot be double-clicked as batch programs. Their original contents and original
relative paths are preserved for investigation, **not adjusted to run from this
directory**. Do not rename and execute them without reviewing their old build,
ROM, save, and machine-path assumptions.

The wrappers covered English-beta rollback modes, the early lake and four-arena
combat pilots, old complete-frame/performance comparisons, the previous debug
launcher, machine-specific Ghidra shortcuts, and alpha release preparation.
None is required to run a current portable through its normal EXE launcher.

References to those root BAT names in older documents describe their historical
locations. Consult this archive instead. Historical Markdown in `docs/` remains
at its original path so existing research links and tool references stay intact.

## What was deliberately retained

- All renderer, UI, and game source, including the lake implementation.
- All automated tests, including lake animation/control and packaging tests.
- The underlying diagnostic and analysis tools under `tools/`.
- `tools/package_alpha.py`: current packagers still import its audit helpers.
- `packaging/alpha-v01/notices`: the portable recipe still uses these notices.
- Dependency pins, credits, artwork/font provenance, and licenses.

No portable installation, local save, ROM, BIOS, build cache, or experimental
worktree is part of this cleanup. Source history and these text copies make
the launch-wrapper retirement recoverable without deleting local experiments.

For current entry points, use the [repository README](../../README.md) and
[tool index](../../tools/README.md).
