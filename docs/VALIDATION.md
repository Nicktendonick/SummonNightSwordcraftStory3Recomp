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

## Native timing and audio

Results recorded on 2026-08-10 from the warm slot-1 checkpoint over 600
windowed presentations on a 165 Hz display:

| Presentation mode | Measured FPS | Present block p50 / p95 | Audio stretch at 8 s | Underruns / overflow drops |
|---|---:|---:|---:|---:|
| Renderer VSync (previous default) | 48.79 | 3,938 / 10,938 us | 1,565 ms | 0 / 0 |
| Native frame pacer (new default) | 59.76 | 250 / 1,745 us | 0 ms | 0 / 0 |

The reusable host now leaves renderer VSync off unless
`GBARECOMP_VSYNC=1` is explicitly set. `GBARECOMP_NO_VSYNC=1` remains a final
override for existing diagnostics. The frame pacer remains the sole normal
speed clock at the GBA's native 59.7275 Hz.

After this change, the forced call-depth regression passed and the canonical
4,400-frame new-game trace remained `FULLY_STATIC`. This checkpoint reproduces
the systemic frame/audio pressure but does not reach the first battle; that
scene remains a manual acceptance check until the deterministic trace is
extended through partner selection.

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
