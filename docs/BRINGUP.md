# Bring-up plan

## Phase 0: repository wiring (complete)

- Pin the known Japanese ROM and its header metadata.
- Use `gbarecomp` and `recomp-ui` as submodules.
- Provide a ROM-free CMake host application and launcher seam.
- Seed conservative entry points and a csm3 symbol importer.

## Phase 1: static corpus (initial trace complete)

1. Build the `gba_recompile` tool in the framework submodule.
2. Run `tools/import_csm3_symbols.py <path-to-csm3>` and review every imported
   address before committing metadata.
3. Run `tools/generate.ps1`; generated ROM-derived files remain local.
4. Add verified jump tables, IWRAM code-copy mappings, and data ranges to
   `symbols/swordcraft3_jp.toml` as the runtime reports misses.

### Current generation result

The first conservative pass imported 2,486 address-bearing csm3 labels and the
current analyzer emits 48,708 entry/resume points across 32 shards. Four
startup code-copy regions, the 36-entry MP2K player jump table, and two
script-callback tables are described as reviewed game metadata in
`symbols/swordcraft3_jp.toml`.

Generated ROM and BIOS translations remain ignored local artifacts. The BIOS
translation is written to `build-debug/generated_bios` through the reusable
`GBARECOMP_GENERATED_BIOS_DIR` framework seam instead of contaminating the
framework source checkout.

## Phase 2: validation

- The initial 10,000-step headless boot trace passes with the verified real
  BIOS and ROM: `FULLY_STATIC`, zero dispatch misses, zero interpreted
  instructions, and zero unhandled I/O accesses.
- The checked-in 4,400-frame input trace skips the opening, starts a new game,
  and selects the male protagonist. It passes with
  `GBARECOMP_STRICT_STATIC=1`. Strict mode disables cache loading and aborts on
  the first interpreter bridge.
- Assist validation confirms exact 2x and 4x guest-frame/presentation ratios
  and an approximately 10x ratio at the 10x setting. The validation host reached
  147.74 guest FPS (2.47x real GBA speed) in the short 10x startup sample.
- A slot-1 snapshot saved at guest frame 6,151 restores in fresh strict-static
  processes. Two independent 120-frame continuations produced the identical
  framebuffer SHA-256
  `6082CAD71A52457A0CD744C6C343B2A5014C54CC7718C39489A9C23663A2192A`.
- The deterministic Assist route exercises save, load, and rewind. A longer
  strict-static walk/interact continuation reaches guest frame 16,151 with zero
  misses. It currently stops at partner selection because the generic walker
  does not make the game-specific shoulder-button choice.
- That continuation exposed interrupt resumes inside `0x08003F9E..0x08004050`
  and `0x080060BC..0x08006106`; both reviewed ranges are now in the game
  metadata rather than the reusable runtime.
- Run `tools/validate.ps1` to reproduce that acceptance trace; add
  `-CaptureFrame` to write an ignored framebuffer snapshot under `validation/`.
- Run `tools/validate_assist_runtime.ps1 -BuildDir build-assist` for the
  isolated Assist campaign and timing CSVs under `validation/assist-runtime/`.
- Treat this as a bounded startup baseline, not whole-game coverage.
- Add deterministic input traces that reach the title screen, new game flow,
  combat, menus, saving/loading, and representative late-game scenes.
- Compare frame, audio, DMA, interrupt, and EEPROM behavior with a reference
  emulator/oracle.
- Promote only reproducible, game-independent fixes to the framework branch.

## Ownership rule

Game facts and presentation policy stay here. ARM/Thumb translation, GBA
hardware modeling, generic launcher seams, and reusable diagnostics belong in
`gbarecomp` on its dedicated feature branch.

### Current framework branch changes

The `feature/swordcraft3-reusable` branch currently contains two game-agnostic
CMake additions:

- `GBARECOMP_GENERATED_BIOS_DIR` lets a parent game keep private BIOS-derived
  translation output in its build tree.
- `gbarecomp_stage_mingw_runtime(<target>)` stages the dynamic SDL2/MinGW
  runtime beside a development executable; static release targets are skipped.

The game repository supplies the build-local BIOS path and opts its executable
into DLL staging. No Summon Night addresses or policies are embedded in the
framework changes.
