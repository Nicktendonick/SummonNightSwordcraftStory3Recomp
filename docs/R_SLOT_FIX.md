# R-slot arena identification fix — 2026-09-14

**September 19 correction:** the grounded-only identity below omitted upper
scenery exposed while jumping. [Manig/airborne notes](MANIG_BATTLE_WIDESCREEN.md)
supersede its near-map ranges and hashes. HUD storage remains excluded.

The usual `Launch Combat Renderer Test.bat` now keeps the verified forest wide
while cycling the six R slots. Extra native correctness redraw remains OFF.

## Cause and correction

The former near-arena hash covered VRAM `0x3800..0x47ff`, including changing
HUD glyphs: the Guard label at `0x4500..0x45bf`. Selecting empty ability slots
cleared that label and changed the hash, triggering unknown-arena fallback.

The fix authenticates only sampled scenery tile rows. The MMIO ring confirms
normal BG0 VOFS=32 during gameplay. Screen Y 19..124 samples source Y 51..156,
enclosed by source tile rows 6..19. Pause shifts the band and scroll together.
`custom_battle_identity.h` hashes these two 32-column blocks' selected rows:

- `0x3980..0x3cff` (896 bytes).
- `0x4180..0x44ff` (896 bytes).
- Combined SHA-1: `b649c3c5aa90b71aed5dae28449d9eff8ee40576`.

The far-map and layout authentication remain. Per-line stability checks use
the same near-row ranges. Near-margin samples outside source Y 48..159 are
rejected. The frame guard applies only to extended gameplay rows, not the HUD.
No graphics bytes are changed, and unrelated arenas are not authorized.

Two intermediate builds used an incorrect 128-pixel source-row limit. Tests
detected native fallback; MMIO evidence resolved the scroll offset before the
final 160-pixel boundary was accepted. These were not accepted final builds.

## Validation

Final executable SHA-256:
`4ab6dc4c56a7a7ba603f07a002dc088bdaad4db4aca4b911445e31b34ea1e226`.
Build log: `validation/build-r-slot-fix-3.log`.

`tools/assess_combat_captures.py --expect-fixed` runs the existing TCP server
with isolated battery files, restores five snapshots, checks native/host
screenshots, then presses R eight times and L six times. Every input frame is
checked, not just settled screenshots. Original snapshots remain unchanged.

- `validation/r-slot-fixed-384-v3`: all 252 cycle frames stay wide.
- `validation/r-slot-fixed-284-v3`: all 252 cycle frames stay wide.
- Formerly narrow forest snapshot 5975 is now wide at both widths.
- Unsupported rocky VICTORY still stays native as intended.
- Critical, paused and attack-effect captures pass 90-frame comparisons each
  in `validation/r-slot-regression-*`. Native pixels and sampled guest-state
  hashes match combat-disabled references. Critical final full-host RGB also
  matches its separately reviewed pre-fix reference.
- Eight focused CTests pass, including synthetic scenery-identity tests.

The new unit test changes every excluded byte in the old near allocation,
representing arbitrary future HUD text, and verifies unchanged scenery
identity. Changes to actual scenery are still rejected. Buffer and source-row
boundaries are tested without embedding ROM content.

## Weapons and items

New labels in the excluded HUD storage cannot cause this same false arena
mismatch. This save contains the starting hammer and empty ability slots:
L input was exercised, but this is NOT validation of switching among several
distinct weapons, acquiring items, or rendering every future spell effect.
New effects may still need layout/HUD support. A populated-inventory snapshot
is the next useful broader gameplay test. Unsupported arenas remain native.

The R fix is game-owned. Accompanying engine changes are the earlier reusable
immutable wide-replay API on its dedicated branch. No ROM, BIOS, save, capture
imagery, generated game code or executable belongs in these Git changes.
