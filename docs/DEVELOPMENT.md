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

## ROM-free Windows checks

The October 16 clean-clone check retrieved both pinned submodules, compiled
the game-repository test targets without private inputs or old object caches,
and passed all 37 checks then registered. The October 17 expanded suite passed
**38 registered game checks** (29 C++ test programs and nine Python suites).
The nine Python suites contain **86 individual tests**. This count does not
include the engine's separately registered test suite. Private playable-build
verification is reported separately below.

The tested host used native Windows CMake 4.4.0, Ninja
1.13.0.git.kitware.jobserver-pipe-1, MinGW-w64 GCC/G++ 14.2.0, Python 3.13.1,
and SDL2 2.30.11. A separate SDL2-upgrade experiment is not silently substituted
for this baseline. CMake requires 3.20+, a C++20 compiler, and Python 3.11+ for
the source/packaging tests.

Install the MinGW64 compiler and SDL2 development package, and native CMake,
Ninja and Python. In PowerShell, from the recursively cloned repository:

```powershell
$toolchain = 'C:/msys64/mingw64' # adjust to your MinGW64 installation
$env:PATH = "$toolchain/bin;$env:PATH"
cmake -S . -B build-check -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  "-DCMAKE_C_COMPILER=$toolchain/bin/gcc.exe" `
  "-DCMAKE_CXX_COMPILER=$toolchain/bin/g++.exe" `
  "-DCMAKE_PREFIX_PATH=$toolchain" `
  "-DGBARECOMP_MINGW_RUNTIME_BIN=$toolchain/bin" `
  -DGBARECOMP_COMPILER_CACHE=OFF
cmake --build build-check --target swordcraft3_tests --parallel 2
ctest --test-dir build-check -L swordcraft3-rom-free --output-on-failure --no-tests=error
```

Each command must succeed before continuing. If tools are not on PATH, invoke
their full paths and pass `-DCMAKE_MAKE_PROGRAM=C:/path/to/ninja.exe` and
`-DPython3_EXECUTABLE=C:/path/to/python.exe` when configuring. CMake fetches
pinned toml++ v3.4.0 unless the engine is given an existing include directory;
the initial configure therefore needs network access.

Use the named test target, not an unqualified build of all game executables:
those require private generated code. A missing generated-BIOS placeholder is
expected for these synthetic tests, not a valid substitute for the BIOS in a
playable release. Assertions remain enabled for the C++ test programs even in
optimized configurations.

The [ROM-free CI workflow](../.github/workflows/rom-free-tests.yml) uses the same
target and test label on a Windows runner with MinGW64 packages. Its actions
are pinned and its token is read-only. No ROMs, BIOS, saves, private captures,
or generated game code are required or uploaded. The workflow has been
prepared locally; **a hosted GitHub Actions run remains to be verified after
the changes are approved and pushed**.

When configuring with MSYS2's MinGW64 dependencies, provide the native prefix,
for example `-DCMAKE_PREFIX_PATH=C:/msys64/mingw64`, and put its `bin` directory
on the process PATH. Specifying `SDL2_DIR` for the launcher alone is insufficient:
the engine performs a separate lookup. Check the configure log says **SDL2 found**.
With the desktop launcher enabled, configuration now fails if the engine would
use its headless stub. This negative case was checked separately. Intentional
headless configuration requires explicitly disabling `SWORDCRAFT3_RECOMP_UI`.

Keep the compiler's runtime DLL directory on PATH while running development
tests. A missing `libgcc_s_seh-1.dll` / `libstdc++-6.dll` is a loader failure,
not evidence that a test ran and found a gameplay bug. Do not download individual
DLLs from unrelated websites. Portable packaging must stage the matching DLLs.

## Playable build and packaging status

On October 17, the build helper completed fresh Japanese/English 1.1.0f and
BIOS generation and both Release engines from local game revision `400942b`
and the dependency pins above. No old guest object cache was reused. Its portable
candidate passed dependency/ZIP checks and targeted private runtime regressions.
This is **not final release certification**: see [coverage and open gates](RELEASE_TESTING.md).
Later source revisions require their own clean build record and candidate.

From a clean committed checkout, using your own verified private inputs:

```powershell
./tools/build_portable.ps1 `
  -JapaneseRom 'C:/private/swordcraft3_jp.gba' `
  -EnglishRom 'C:/private/Swordcraft Story 3 English 1.1.0f.gba' `
  -Bios 'C:/private/gba_bios.bin' `
  -BuildDir 'build-portable' -Jobs 2
python -B tools/package_portable_release.py --build-dir build-portable
```

Use `-CMake`, `-Ninja`, `-Python`, `-Git` and `-CompilerPrefix` if the tools are
not on PATH or are installed elsewhere. Use a short `-BuildDir` to avoid Windows
object/cache path limits. Nested checkouts can package into their owning project
with `--owner-root C:/path/to/owner` and an explicit `--build-dir`. The packager
also accepts `--git` and `--strip` tool paths. It creates a uniquely named new
candidate and a separate extracted test copy; it never overwrites a played install.

The helper builds `gba_recompile`, regenerates the private BIOS, reconfigures
the runtime to include it, and generates Japanese and English code from their
separate verified inputs. It records exact revisions and hashes in
`portable-build.json`. The packager rejects changed binaries, dirty/mismatched
source revisions, old translation metadata, and cached-guest-object builds.

To verify the ZIP's allowlist and hashes later, no old build tree is needed:

```powershell
python -B tools/package_portable_release.py --verify 'C:/path/to/candidate.zip'
```

This is integrity/packaging verification, not a digital signature, permission to
distribute derived code/artwork, or proof that every game route works.

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
[modding entry points](MODDING.md). [Historical documentation](history/README.md)
remains available for investigation, not as a current launch recipe.
