# Critical-hit widescreen (2026-09-09)

The supplied forest critical-hit snapshot (frame 9933 in session
`20260909-102638-718-beta`) switches from Mode 0 to Mode 1. The previous
game policy rejected all non-Mode-0 layouts, pillarboxing both scenery and
HUD. This was not a window-size change or lost background data.

## Implementation

- Authenticate the reviewed Mode 1 signature: BG0 `0x0000`, BG1 `0x450B`,
  BG2 `0x4385`, BG0/1/2 enabled, no forced blank or hardware windows.
- Continue the existing text scenery policies for BG0/BG1 and authenticate
  the completed HUD before filling its tan wings.
- Leave BG2 to the existing shared affine renderer. Its signed-coordinate
  extrapolation and hardware wrap-OFF canvas bounds prevent repeated copies.
  Do not interpret this canvas as a regular tilemap or loop it like scenery.
- Decouple scenery eligibility from guest object expansion. Critical-hit
  culling and native OBJ clipping retain their previous behavior. This change
  does not modify camera positions, collision, guest VRAM, or palettes.
- Unsupported affine layouts still fail closed. No additional gbarecomp or
  recomp-ui changes were needed for this critical-hit exception.

## Use / rollback

Restart the current English-beta or Japanese executable in `build-beta`.
`Launch Debug Capture.bat` uses the updated beta executable automatically.
For the former critical-hit pillarboxing, run
`Launch Beta - Previous Critical Hit.bat`, which sets
`SWORDCRAFT3_CRITICAL_WIDESCREEN=0` for that process only.

## Verification

`tools/validate_critical_widescreen.py` compares an isolated old executable,
the new executable, and the rollback switch at the SAME width. Local evidence
is in `validation/critical-hit-fix-20260909/first/report.json` (Git-ignored).

- Critical snapshot continuation: frames 9935..10085, every five frames,
  at 240, 284, and 384 pixels; includes return from Mode 1 to normal battle.
- Every sampled native-center pixel and guest-state trace matches before/after.
- Full-image rollback matches the previous executable exactly.
- Critical HUD wings match authenticated native gutters at both wide sizes.
- Isolated BG0/BG1/BG2/OBJ frames inspected at 384 pixels; the affine impact
  is a single canvas, with no repeated edge copy in the reviewed samples.
- Rocky battle and overworld regression snapshots at 384 pixels remain
  pixel-identical, including margins. Camera limits and edge overlays are
  deliberately not changed by this patch.
- Game presentation unit tests pass, including mode/signature/wrap/window
  rejection cases. Both English-beta and Japanese targets build successfully.

The session input trace contains backwards frame jumps from save/load use.
Tests therefore continue each saved snapshot with released controls, not the
entire recording. This is not all-critical-effects coverage or a certification
of existing native-versus-wide guest parity; those broader questions remain.
Japanese is build-tested; snapshot replay evidence is English beta.

Validated English-beta SHA-256:
`8A65644761DD9A8196E28F85BCCDA499DB21A00D26D88D72756F4C717A37457A`

Validated Japanese SHA-256:
`CE574C82DF61460B629650A8A59C9FB61EEB5382EEFD131D9B69CCE005771585`
