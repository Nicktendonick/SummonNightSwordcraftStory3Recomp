# Select-to-Guard GBA ROM hack — experimental v0.1

Standalone ARM7TDMI/Thumb code, not a PC-port hook or emulator cheat. The PC
port, renderer and existing portable folders are not changed by this work.

## Player behavior

Hold Select to request the game's normal Guard during manually controlled
combat. Release it to stop, just as with the normal Guard button. The selected
R ability stays selected; normal R cycling and B use remain available.
Native restrictions still apply: this does not grant invulnerability, cancel
every action, bypass airborne restrictions or alter damage calculations.

Select no longer enables auto-battle during normal manual combat. Cancellation
of already-active auto-battle retains its original path. Field, menu, scripted
and link-mode contexts fail closed to native behavior. This is always enabled
in the patched ROM; there is no PC-style Mods checkbox or new in-game menu.

## Exact supported bases

Both files must be 33,554,432 bytes. Reject other revisions or already-modded
files instead of forcing a patch.

| Patch | Source SHA-1 | Source CRC32 |
| --- | --- | --- |
| Japanese | `3f5253fcf57e07ce52472bd29a61d16b98a12376` | `12afae5d` |
| English 1.0.6.f | `6753a22a096b8adaa3a869333b99fcfe29ba1fec` | `c76631a9` |

English installation order is Japanese original -> English translation
1.0.6.f -> English Select-to-Guard patch. Do not stack the Japanese Guard patch
with the English translation or apply both Guard patches. Other translation
revisions require revalidation, not a renamed BPS file. These patched ROMs are
for ordinary GBA emulators/compatible hardware, not the hash-checked PC port.

## Investigation and implementation

Ghidra was used to inspect the original ability dispatcher and auto-battle
routine, and to check references into the replaced instruction spans. Its
loaded project was an older English beta, so it was a research reference, not
the authority for current ROM offsets. All five reviewed routine spans were
authenticated against BOTH actual supported ROMs. All 22 interior overwritten
instruction addresses and the cave entry had no incoming references in that
Ghidra reference. This static result is not proof about every dynamic branch.

`build.py` contains readable assembly and the exact source identities. It
requires the expected full ROM hash, routine hashes and trailing zero padding.
Five short jumps go to five small Thumb routines beginning at cartridge
address `09FC0000` (file offset `01FC0000`). No ROM expansion or persistent RAM
allocation is used. The header, native Guard update routine and all bytes
outside those five hooks and payloads remain unchanged.

| Hook | Role |
| --- | --- |
| `080272E4` | Physical manual input: held Select supplies held B before action history. Preserve AI/event paths. |
| `08029964` | Suppress Select enabling auto-battle only in authenticated manual combat. |
| `08042692` | First ability lookup locally treats held Select as Guard. |
| `080426D6` | Second ability lookup locally treats held Select as Guard. |
| `0804285A` | Avoid replacing the selected R slot or resetting its HUD during dedicated Guard. |

Authentication requires root `03006AC0 == 03000000`, lifecycle
`03006AB4 == 4`, mode byte `0300000C == 2`, submode `0300000D != 3`,
pause `0300000F == 0`, and auto-battle `03000012 == 0`. Ability hooks also
require actor `030008C0` and held Select in `0300594C`. The physical input
hook additionally verifies its incoming input-source pointer.

The long-jump trampolines preserve scratch registers, LR and stack balance.
Returns preserve the flags and register values that the next original
instructions expect. The original hold/release function at `08049018` is
untouched. Extra native instructions have a cycle cost; no zero-cost claim or
hardware timing certification is made.

## Rebuild

Use Python 3 with `keystone-engine==0.9.2` and `capstone==5.0.6`. Dependencies
may be installed into `validation/guard-romhack/tooling/python` (the builder
checks that private location first) or an isolated Python environment.

```
python tools/guard_romhack/build.py "PATH/TO/SUPPORTED.gba" --out "PRIVATE/NEW/FOLDER"
```

The output directory must not already exist. The builder never modifies the
input ROM. It produces a private patched ROM, BPS and build manifest. Only the
BPS and original source/documentation may be distributed; do not commit or
ship the generated ROM, BIOS, battery data or emulator states.

## Validation completed 2026-10-01

- `test_cpu.py`: 38,280 differential cases execute the actual Thumb bytes
  against original instructions plus the established Guard model. Covers all
  1,024 button combinations, six slots, and 36 negative contexts; compares
  registers, flags, stack balance and persistent game RAM. Executed opcode
  audit admits only ordinary 16-bit ARMv4T Thumb-1 instructions.
- `test_patch.py`: independent BPS decoding reproduces each tested ROM;
  deterministic rebuild; exact changed-span audit; rejects wrong source,
  already-patched source, truncation, corrupt patch and corrupt target CRC.
- `test_emulator.py`: unmodified mGBA 0.10.5 core, commit
  `26b7884bc25a5933960f3cdcd98bac1ae14d42e2`, official private GBA BIOS.
  Both original and patched Japanese/English ROMs booted, loaded a copied
  battery in memory and reached the first rocky battle through normal inputs.
  No gameplay memory writes, PC runtime hooks or screenshots were used.
  In both languages, 120-frame Select holds and releases matched the native
  B-Guard flag/timer timeline. Real R input selected slot 1, which survived
  dedicated Guard. Native pause/resume, airborne restrictions and field
  Select also passed their bounded state checks. User battery was unchanged.

For emulator reproduction, build the official mGBA 0.10.5 static core with
GBA enabled and minimal dependencies, then build `mgba_probe.c` as a private
shared library. The supplied adapter targets Windows/MinGW; its relative
tooling, ROM, BIOS and fixture paths match this project's private checkout.
`test_cpu.py` additionally needs `unicorn==2.1.4`. Tests are not required to
apply the BPS. `test_emulator.py` expects the project's copied early-game
battery route; no copyrighted fixture is included in the source archive.

Private evidence: `validation/guard-romhack/{cpu,patch,emulator}-report.json`
and per-language emulator reports. Machine checks do not certify the whole
game: physical GBA/flashcart operation, link play, every boss/script, existing
auto-battle save-state cancellation and long-session timing remain untested.
Use a copied ROM and backed-up battery save for human testing. Start the
patched ROM normally; don't rely on save states created by a different ROM.

## Tools

Ghidra (reverse engineering), Keystone (assembly), Capstone (disassembly),
Unicorn (CPU differential tests), mGBA (independent GBA gameplay tests),
Python, GCC, CMake and Ninja. Their original licenses remain with their
respective projects. No third-party tool binaries are included in this patch
package. No ownership of the original game or translation is claimed.
