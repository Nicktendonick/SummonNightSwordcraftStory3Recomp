# Local Ghidra workspace

This is analysis tooling, not a rendering change. The target remains **12:5**.
The GBA/ROM databases and every exported decompilation stay in private ignored
directories under the owning Documents project. No ROM or BIOS is uploaded.

## Pinned installation

- Ghidra **12.0.4**, official NSA release. Archive SHA256:
  `c3b458661d69e26e203d739c0c82d143cc8a4a29d9e571f099c2cf4bda62a120`.
- Portable Eclipse Temurin JDK **21.0.12.1+1**. Archive SHA256:
  `f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e`.
- [pudii/gba-ghidra-loader](https://github.com/pudii/gba-ghidra-loader), **1.1.0**,
  source `70fc62c388741bb03e5f4f68eaf0b491b223430d`. Rebuilt against 12.0.4;
  the original release binary targets 12.0.2. Java source unchanged.
- [13bm/GhidraMCP](https://github.com/13bm/GhidraMCP),
  **v0.2.2+ghidra12.0.4**, source `ffe716235a292e0c19714525bd0b6fab4925829b`.
  Release SHA256 `f10adcc8ae4aab1ddc82240f96cca0f1fa0412d0cbab6ff83759613f8e1ee03d`.
  Selected for its matching release and headless-capable server class, rather
  than LaurieWired's currently published Ghidra 11.3.2 build. Upstream code is
  unchanged; the game repository provides its lifecycle/read-only wrapper.

All installation files, sources, Java settings, caches and temporary files are
under `<owner>/.tooling/ghidra`. Reproduce or verify with
`tools/ghidra/install_ghidra.ps1`. `-RebuildLoader` requires all Ghidra processes
to be closed. No global Java install or system PATH change is needed.

## Open the projects

At the top of the Documents project, double-click either:

- **Open Ghidra - Japanese.bat**
- **Open Ghidra - English Beta.bat**

The wrappers open separate projects in `<owner>/validation/ghidra/projects`.
In Ghidra's project window, double-click the imported `.gba` program to open the
CodeBrowser. Close that project's GUI before using its headless MCP connection
if Ghidra reports a project lock. The GUI permits analysis edits, but does not
write those changes back to the input ROM.

Input identity and checked symbol seeds are recorded in
`<owner>/validation/ghidra/{jp,beta}/input.json` and `checked-seeds.tsv`.
The Japanese ROM must match the `csm3` recorded SHA1 before importing seeds.
Beta seeds are accepted only where the **whole interval to the next known
symbol** matches the Japanese ROM. The final unbounded seed is omitted.
Changed intervals are recorded instead of being silently relabeled.

## Assistant connection

Codex server name: **swordcraft3_ghidra**, default program: **English beta**.
Only its launcher reference/timeouts are stored in the user's Codex settings;
all implementation/data stay in the project. Project-local config copies are
also ignored by Git. The current desktop task may need its MCP connection
restarted, or a new task, before new tools become available directly.

`sc3_ghidra.py mcp` supplies the verified tool list without starting Java. On
the first analysis query it launches the installed upstream server against the
existing database with `-readOnly -noanalysis`, then connects the upstream
Windows stdio bridge. This is **live decompilation**, not a fixed text export.

Security/lifecycle:

- Loopback only; no firewall rule or LAN listener.
- Random per-session API key in child-process environment, not arguments/logs.
- Wrapper allowlist for inspection/decompilation only; patching, renaming,
  arbitrary script execution and cross-instance port overrides are rejected.
- Native Ghidra database saves disabled during headless MCP use.
- Child server exits when the connection closes or its owning process dies.
- Ghidra is not launched for unrelated tasks that merely enumerate the tools.
- Pseudocode returned to the assistant becomes part of that task's context;
  "local" does not mean the model never receives the requested excerpts.

The upstream server has mutation APIs internally. Do not bypass this wrapper
or enable the GUI MCP plugin if you want to preserve this read-only boundary.
The extension is available in the GUI, but that separate server is not needed.

## Verification and limits

`CheckSc3Extensions.java` checks loader/MCP linkage without a ROM or listener.
`VerifySc3.java` checks the GBA memory regions, processor, selected loader and
decompilation of five known functions (field update/load, archive lookup and
battle scheduler). Private outputs are in `verification.json` and focused `.c`
files, never the public repository.

`tools/ghidra/test_mcp.py --variant beta` performs a real MCP handshake, verifies
the loaded ROM/language, lists inspection tools, decompiles two field functions,
rejects write/port-override requests, and checks clean process shutdown. Repeat
with `--variant jp` for the original database. Results remain private in
`mcp-verification.json`. It also compares database storage hashes before and
after the session.

Verified on 2026-09-22:

- Both ROMs imported with the GBA loader and passed all five focused checks.
- Both broad automatic analyses reached the 900-second timeout. The saved
  databases are usable but **partial**, not complete game decompilations.
  `sc3_ghidra.py status --variant jp` (or `beta`) records this limitation in
  the existing private verification report without rerunning analysis.
- Japanese standalone MCP passed both initial and cached-schema/deferred-start
  runs: 26 inspection tools, two live decompilations, forbidden requests blocked,
  unchanged database storage, and clean shutdown.
- The desktop task's actual beta MCP connection returned the correct program
  metadata and live pseudocode for `08093994` and `08094a4c`; a cross-instance
  port override was rejected. These tools are already available in that task.
- A separate beta test process could not run concurrently because the active
  desktop MCP connection held the project lock. This is expected exclusivity,
  not a corrupted database. Do not remove lock files while a connection is live.
- GUI launcher files are provided; interactive CodeBrowser operation has not
  been separately tested. Disconnect the corresponding MCP session before
  opening the same project in the GUI.

Automatic analysis is bounded to 900 seconds per ROM. It may discover false
functions in embedded assets, miss indirect calls, or report ARM/Thumb conflicts.
The function count is **not** an authoritative game-code inventory, and this
setup does not prove that every function has been found. Follow exact assembly
and runtime-state evidence for edits. No pixel/image assertions are used.

Next investigation: authenticate the shared field lifecycle and loaded layer
descriptors, then replace per-room allowlisting with verified general rendering
support. Installing Ghidra alone does not widen additional rooms.

To disconnect, remove `swordcraft3_ghidra` from Codex MCP settings (or use
`codex mcp remove swordcraft3_ghidra`). The project-local config copies should
also be disabled if that project is subsequently trusted. Keep the private
databases if you want to retain analysis notes. Nothing here is pushed by setup.
