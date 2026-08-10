# Validation status

Results recorded on 2026-08-09 using the `build-assist` RelWithDebInfo build,
the verified Japanese ROM, and the verified retail GBA BIOS.

## Assist Tools

| Requested mode | Presents | Guest frames | Guest frames/present | Guest FPS | Real-speed multiple |
|---|---:|---:|---:|---:|---:|
| 1x | 60 | 59 | 1.00 | 13.61 | 0.23x |
| 2x | 60 | 119 | 2.00 | 31.14 | 0.52x |
| 4x | 60 | 239 | 4.00 | 63.94 | 1.07x |
| 10x | 60 | 609 | 10.13 | 147.74 | 2.47x |

The selected multiplier controls guest frames per paced presentation correctly.
Actual emulation speed remains CPU/workload limited, so requesting 10x does not
guarantee 10x wall-clock speed on every scene or machine.

The deterministic Assist script saved slot 1, advanced, restored the snapshot,
built fresh rewind history, and rewound successfully. The snapshot also loaded
in two fresh strict-static processes; both 120-frame continuations ended at
guest frame 6,271 with zero dispatch misses and identical framebuffer hashes.

## Windowed call-depth regression

Windows Error Reporting identified the first-cutscene failure as an intentional
abort in `runtime_call_push_return`: present-in-place windowed execution had
grown the generated host call chain to its fixed 1,024-entry limit. The reusable
runtime now unwinds and redispatches at a safe VBlank when depth reaches 512.

A forced-threshold regression set the limit to 1 during a 120-presentation
windowed boot. It exercised eight safe unwinds and exited normally with zero
dispatch misses and zero interpreted instructions. The standard 4,400-frame
strict-static new-game route also remained `FULLY_STATIC` after the change.

## Static coverage

- Canonical new-game route: 4,400 frames, `FULLY_STATIC`.
- Restored-state walk/interact continuation: 10,000 additional frames through
  guest frame 16,151, `FULLY_STATIC`.
- The continuation exposed two asynchronous return gaps. Reviewed resume ranges
  for `0x08003F9E..0x08004050` and `0x080060BC..0x08006106` fixed them.
- Final continuation image: `extended_walk_10000.png`; it is parked at the
  partner-selection menu.

## Remaining coverage boundary

The next deterministic trace must make the game-specific partner selection and
then cover exploration, map transitions, the first battle, in-game saving, and
post-battle transitions. Rendering/audio comparison against a reference
emulator and longer save-state/rewind soak tests also remain open.
