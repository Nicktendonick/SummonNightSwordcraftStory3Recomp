# Upstream renderer import

Source: https://github.com/mstan/SuperMetroidRecomp
Pinned commit: `3b419d00dd29696947fc311991c7c59a5b9861b7` (retrieved 2026-09-09).

The six `sm_*` source/header files and `UPSTREAM.md` are unmodified reference
imports. `LICENSE` is the upstream PolyForm Noncommercial License 1.0.0,
including its copyright/clarification. Retain these notices when distributing
these files. This import is for the owner's noncommercial port experiment.

The SNES/Super Metroid decoder is NOT linked into the GBA executable. It expects
SNES PPU state, planar tile formats, Super Metroid WRAM addresses and SNES room,
metasprite/effect layouts; changing a few dimensions would not make it a GBA
renderer. Its capture/replay contract is being adapted in stages.

Initial GBA adaptation: immutable per-visible-line IO, palette, OAM, VRAM and
hidden affine-state capture, followed by native replay without guest execution.
The existing GBA pixel kernel is reused for this first isolation milestone.
This is not yet the independent world/room renderer or offscreen OBJ recovery
implemented by the upstream Super Metroid-specific code.
