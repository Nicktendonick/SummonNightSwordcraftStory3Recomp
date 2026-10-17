# Developer workflow

## Source identity

Use a recursive Git clone, including the exact `gbarecomp` and `recomp-ui`
submodule commits selected by the game repository. Do not replace them with
upstream HEAD or reuse old generated objects while claiming a clean build.
Automatic GitHub source archives do not include populated submodules.

The release-readiness baseline is merged PR #12, game commit
`83990179a92c89b8e01d5853d79529fb1cb616c1`, with engine
`3a7e974759b2a79ad4144396d571da6f004ba393` and UI
`0aae3c6cfbbf10ba5c4e37165e13a116fa3a820f`.

## Build verification in progress

The October 16 clean-clone check successfully retrieved both pinned submodules.
It configures with native Windows CMake, Ninja, MinGW-w64 GCC/G++ 14.2,
Python 3.13, and the installed SDL2 development package. The first test build
is in progress; a complete fresh Japanese/English playable build is **not yet
certified** by this record. Full reproducible commands will accompany that
verification, rather than directing developers to a private object cache.

When configuring with MSYS2's MinGW64 dependencies, provide the native prefix,
for example `-DCMAKE_PREFIX_PATH=C:/msys64/mingw64`, and put its `bin` directory
on the process PATH. Specifying `SDL2_DIR` for the launcher alone is insufficient:
the engine performs a separate lookup. Check the configure log says **SDL2 found**;
**host_window will stub out** is not an acceptable playable configuration.

Keep the compiler's runtime DLL directory on PATH while running development
tests. A missing `libgcc_s_seh-1.dll` / `libstdc++-6.dll` is a loader failure,
not evidence that a test ran and found a gameplay bug. Do not download individual
DLLs from unrelated websites. Portable packaging must stage the matching DLLs.

## Private inputs and artifacts

See [baserom.md](../baserom.md) for the original ROM and BIOS identities. English
1.1.0f must match SHA-1 `99ca58319b050c238b1cb61cbc6db7b418b5472c`.
Translation code generation uses its revision-specific override configuration;
Japanese must use its own corpus. Translation save states are revision-bound.

Place private build output in ignored `build-*` directories. Keep ROMs, BIOS,
patches, saves, captures, and generated guest code out of source commits and
public CI. Public tests must use synthetic/source/state fixtures, not uploads of
game memory or copyrighted input images.

## Contributing safely

1. Record the game, engine, and UI revisions and the intended behavior change.
2. Keep game-specific changes here and reusable changes in the appropriate
   dependency repository. Preserve unrelated local changes.
3. Run relevant ROM-free tests, then targeted private state/input regressions
   when behavior changes. See [AGENTS.md](../AGENTS.md).
4. State exactly what passed and what remains untested. Automated state checks
   do not prove human visual quality or whole-game compatibility.
5. Keep cleanup, gameplay changes, dependency updates, and release packaging
   separately reviewable. Publication and external test coordination are
   separate decisions, not consequences of building successfully.

See [tool index](../tools/README.md) and
[historical documentation](history/README.md) for investigation leads.
