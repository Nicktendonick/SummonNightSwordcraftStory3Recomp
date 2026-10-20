# English 1.1.1f test candidate

This is a separate local update from merged game baseline `b0a64ca`, retaining
engine `3a7e974` and UI `be0cc15`. It does not replace the working 1.1.0f ZIP,
publish a release, upgrade dependencies, or import the Linux/macOS preparation.

## Inputs and bounded compatibility evidence

- Original Japanese SHA-1: `3f5253fcf57e07ce52472bd29a61d16b98a12376`.
- Supplied BPS SHA-256: `5e0693b1899306ab8f61157efc6d9cc98b4e6a7c5a7ffb954470c6040405ac4d`.
- English 1.1.1f SHA-1: `65885d763f77648ac733daac69de15bc1ccb0ea5`.
- English CRC32: `1001F351`.

The pinned launcher's BPS implementation verifies the patch and creates a new
private ROM, leaving all source inputs unchanged. The independently decoded
in-memory result has the same target identity. Nothing game-derived is committed.

`tools/audit_translation111_inputs.py` verifies exact inputs and BPS checksums,
44 reviewed instruction/metadata/hook spans, and 35 near/far map roles across
the 21 descriptor slots (17 standard arenas plus fallback slots). Those spans
and map sources match 1.1.0f. The field expiry-message literal at `080A4560`
changes from `08004194` to `0800B140`, pointing to the same message bytes;
surrounding field producer/consumer instructions remain identical. Existing
Guard, spell-window, camera, source-map and live-ownership guards stay intact.

This is not a claim that the entire ROM or instruction corpus is unchanged:
742,983 bytes differ from 1.1.0f. Generate the English corpus afresh with
`SWORDCRAFT3_RELEASE111_ROM` and `symbols/swordcraft3_release111.toml`. The
Japanese engine uses its separate original corpus. Never bypass the ROM hash
gate or relabel an old executable/save state to make it appear compatible.

## Player data and regression scope

Extract the candidate into a new folder. English battery data uses
`Saves/english-1.1.1f.eep`; state slots use
`Save States/english-1_1_1f.stateN`. The underscore basename avoids the runtime's
extension replacement collapsing different revisions to `english-1.1.stateN`.
Japanese keeps its own files. Nothing migrates automatically; back up any
battery save before explicitly testing Import. Old save-state compatibility
is not established, and old states must not be silently rewritten for players.

Required new evidence: clean two-language build, unit checks and actual ROM
refusal/routing; cold boot and exact state save/load/replay; launcher patching,
language switching and Graphics; Esc/pause/reset/F10; active Repeat scenery
and Guard behavior across the arena fixtures, plus field behavior. Record any
diagnostic fixture preparation distinctly from fresh in-game entry. Previous
1.1.0f reports and the owner's informal playtest do not count as 1.1.1f passes.

At source preparation, runtime validation is pending. Candidate identity and
test results will be recorded in a separate local handoff after execution.
No clean-PC, weaker-hardware, physical-controller or human audiovisual approval
is implied by automated tests; public distribution review also remains open.
