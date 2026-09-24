# EmeraldRecomp reference review

Read-only review requested alongside publication of the accepted combat
checkpoint. Pinned source: `mstan/EmeraldRecomp` at
`2e4a34020521cfdcd419393dc4aae69b7425fcc4`.

## Architecture distinction

Emerald's current widescreen implementation is not a complete replacement of
the GBA compositor. Its plugin installs map-entry, object-margin and UI-coordinate
providers into the existing expanded PPU. Installation keeps ordinary OAM
native-clipped and provides separately qualified objects for the margins.
Object presentation state resets when the runtime save/load epoch changes.
See [the pinned plugin](https://github.com/mstan/EmeraldRecomp/blob/2e4a34020521cfdcd419393dc4aae69b7425fcc4/src/mods/emerald_adaptive_view_plugin.cpp).

The documented field adapter prioritizes live map edits over ROM map data,
handles connected maps subject to compatible resident resources, and accounts
for camera state being ahead of the displayed scrolling map. Its object layer
distinguishes live actors from dormant presentation poses; it does not expand
guest AI or spawning. Battles remain native-framed. See
[the implementation notes](https://github.com/mstan/EmeraldRecomp/blob/2e4a34020521cfdcd419393dc4aae69b7425fcc4/docs/WIDESCREEN_EXPERIMENT.md).

## Implications for Swordcraft

- Keep Super Metroid as the architectural reference for full-frame ownership.
  Emerald offers useful GBA source/visibility ideas, not a reason to reintroduce
  the hybrid native-center overlay we just removed from combat.
- For field migration, retain publication-time camera/source state rather than
  mixing next-frame object RAM with current OAM. Extend the existing coherent
  capture contract instead of introducing screen-image recognition.
- Treat draw eligibility, resource residency, script invisibility, and actor
  activation as distinct decisions. Do not widen activation to make an NPC draw.
- Invalidate any future presentation cache on restore and map identity changes.
- Connected-map expansion would require traced Swordcraft connection formats
  and resource rules. Emerald's map structures and addresses do not transfer.
- Dormant idle poses would be a separate, explicitly approved feature: they
  cannot be represented as live offscreen simulation.

No Emerald source, assets, hooks or new dependency revisions were imported.
Its screenshot/pixel comparison tests are not adopted: this project continues
using the owner's requested state/source/control-flow assertions. The ongoing
upstream engine/UI integration remains separate and unfinished.
