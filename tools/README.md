# Developer tools

Players should start the portable's **Swordcraft Story 3 Beta.exe**, not these
scripts. Start with [developer instructions](../docs/DEVELOPMENT.md).

## Build and packaging

- `build_portable.ps1`: reproducible Windows Japanese + English 1.1.0f build;
  verifies input hashes, regenerates private code, and records clean revisions
  and output hashes. It does not reuse the old experimental object cache.
- `generate.ps1` / `generate_bios.ps1`: private code generation for verified
  local inputs. Generated game and BIOS code must never be committed.
- `package_portable_release.py`: allowlisted portable candidate construction,
  dependency auditing, manifest, hashes, and archive verification. Its target
  is Japanese + English 1.1.0f, selected by `packaging/portable-beta/target.json`.
  Packaging requires the matching `portable-build.json`; use explicit
  `--build-dir` and `--owner-root` for nested developer checkouts.
- `package_alpha.py`: historical packager **and active shared audit helpers**.
  Other packagers import it; do not delete it based on its name.

Read a tool's arguments and preconditions before use. Several historical
promotion/update scripts target specific existing local test installations;
they are not a general fresh-build or release procedure.

## Renderer regression and diagnostics

- `validate_repeat_scenery.py`: Current/Repeat input and game-state comparison;
  requires compatible private ROM/state inputs and a debug-enabled build.
- `benchmark_combat_tcp.py`: controlled combat performance measurements.
- `validate_f10_capture.py`: capture behavior checks with private test inputs.
- `validate_ui_cleanup.py`: launcher/Esc organization verification workflow.
- `ghidra/sc3_ghidra.py`: historical analysis helper; inspect its configuration
  and ROM variant support before using it for a newer translation.

These tools do not certify human visual quality, physical controllers, clean
machines, or every gameplay route. Observe the state-based verification rules
in [AGENTS.md](../AGENTS.md). Do not publish private diagnostic payloads.

## Historical launch paths

`launch_custom_native.ps1`, `launch_visible_debugger.ps1`, old promotion scripts,
and `prepare_alpha_release.ps1` remain available for investigation and existing
test dependencies. They can assume old nested worktrees, English-beta corpora,
or validation folders. They are not the current portable's entry point.

The 21 old root BAT shortcuts are preserved as non-executable source text under
[docs/history/launchers](../docs/history/launchers/). This retirement does not
disable the normal portable's F10 capture or remove automated lake tests.
