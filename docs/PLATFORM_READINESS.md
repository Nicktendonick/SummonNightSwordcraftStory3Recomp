# Linux and macOS preparation

This is a **developer build-readiness effort, not a Linux/macOS game release**.
The verified Windows portable remains the player-facing candidate. Its release
readiness still awaits clean-Windows launch, physical-controller checks and
human visual/audio review. Those gates have not been waived.

## Scope and evidence

Initial targets are Linux x86-64, macOS Apple Silicon and macOS Intel. Apple
means macOS here, not iPhone/iPad. A passing native compile/link/synthetic test
does not establish that the game boots, renders correctly, sounds right or is
packaged for another computer. This Windows development machine has neither
a configured Linux environment nor a macOS test host.

The separate [platform workflow](../.github/workflows/platform-readiness.yml)
uses `ubuntu-24.04`, `macos-15` (ARM64) and `macos-15-intel`, following
[GitHub's runner labels](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
All results must be attributed to the exact run and commit. Native execution
is pending until such a run succeeds; workflow configuration is not evidence
of a passing build. Minimum supported OS versions are not yet established.

The workflow builds pinned SDL **2.30.11** from source, matching the Windows
baseline version instead of implicitly substituting an SDL3-backed compatibility
layer. It does not upgrade the Windows candidate. No ROM, BIOS, save, translation
patch or generated game code is used or uploaded. It has a read-only GitHub
token and neither publishes nor signs binaries.

Local Windows preparation check: the full link probe built successfully, all
38 existing game checks plus its help/loader check passed (39 total), and both
standalone launcher input checks passed. The shell script's syntax, help and
invalid-job handling were also checked using Git Bash; that is **not native
Linux execution**. The Windows candidate ZIP retained SHA256
`0108996753ef1d1b07c144ba47d59cb63320e2fb530be6a8413feaaad7e8c122`.

## What the new check tests

`swordcraft3_platform_checks` builds the existing ROM-free tests and
`swordcraft3_platform_link_probe`. The probe links the actual game entry point,
custom renderer, presentation, engine and launcher against an empty cartridge
dispatch table from the engine's existing test fixture. Its CTest calls only
`--help`, before asset discovery, SDL initialization or game execution.

**Never package or play the probe.** It deliberately has no recompiled game
or BIOS. It is excluded from the default build and the Windows package allowlist.
The ordinary production targets continue to require locally generated code.

The game now uses the engine's platform-aware runtime archive-link helper, so
Apple's linker is not passed GNU-only archive-group switches. This changes no
guest instructions, combat state, camera policy, widened layers or native-center
contract. Unsupported gameplay scenes keep their existing fallback.

Windows PowerShell debug-launcher tests are explicitly disabled on other hosts,
not counted as successfully exercised. The other Python suites can still test
Windows package formats using synthetic fixture bytes on any OS. Those are not
Linux/macOS packaging checks. SDL virtual-controller and keyboard tests do not
substitute for physical controllers, display drivers or audio devices.

## Reproduce the source checks

Start with a recursive clone and its exact dependency pins. Use CMake 3.24+,
Ninja, a C++20 toolchain, Python 3.11+, SDL2 development files and desktop OpenGL
development support. The engine fetches pinned toml++ on initial configuration.
On Ubuntu, the package names for the ordinary development route include
`build-essential cmake ninja-build python3 libsdl2-dev libgl1-mesa-dev`.
That distro SDL version must be recorded; it is not necessarily 2.30.11.

For the workflow's exact SDL source, use commit
`fa24d868ac2f8fd558e4e914c9863411245db8fd` of
[libsdl-org/SDL](https://github.com/libsdl-org/SDL/tree/fa24d868ac2f8fd558e4e914c9863411245db8fd).
Build/install it into a private development prefix with CMake, using
`SDL_SHARED=ON`, `SDL_STATIC=OFF`, `SDL_TEST=OFF` and `CMAKE_BUILD_TYPE=Release`.
On macOS use the native Xcode command-line toolchain; do not mix Intel and ARM64
libraries. The workflow shows the full dependency/build commands for each host.

From the repository in a POSIX shell, with `sdl_prefix` set to that install prefix:

```sh
sh tools/check_platform.sh --sdl-prefix "$sdl_prefix" --jobs 2
```

With system-provided SDL2, omit `--sdl-prefix`. Use `--build-dir` for a separate
build directory (the launcher tests use its `-ui` sibling). The script applies
the CMake configuration, builds and tests, stopping at the first failure. It
does not edit `CMakeLists.txt`, install system packages or build a portable.
The CI workflow invokes this same script. `--help` lists prerequisites.

The equivalent individual commands are:

```sh
cmake -S . -B build-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$sdl_prefix" \
  -DGBARECOMP_COMPILER_CACHE=OFF \
  -DPython3_EXECUTABLE="$(command -v python3)"
cmake --build build-platform --target swordcraft3_platform_checks --parallel 2
ctest --test-dir build-platform -L '^swordcraft3-(rom-free|platform)$' \
  --output-on-failure --no-tests=error --timeout 120

cmake -S recomp-ui -B build-platform-ui -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$sdl_prefix" \
  -DSNESRECOMP_SDL_BACKEND=SDL2 -DBUILD_TESTING=ON
cmake --build build-platform-ui --target recomp-ui-scripted-key-test \
  recomp-ui-capture-hotplug-test --parallel 2
ctest --test-dir build-platform-ui -R '^recomp-ui-(scripted-key|capture-hotplug)$' \
  --output-on-failure --no-tests=error --timeout 120
```

Stop on any command failure. The SDL engine lookup must report **SDL2 found**;
a headless stub is not a desktop-support result. Keep assertions enabled.
Do not run the source checks as root: file-permission tests rely on an ordinary
user account. Do not run an unqualified all-target build without private inputs.

## Remaining work before a playable preview

1. **Native build evidence:** run the new workflow, fix demonstrated compile/link
   defects in their owning repositories, and preserve Windows regressions.
   Generic engine/UI changes belong in their own feature branches, not game-local
   copies. Exercise both architectures on macOS; no universal binary is promised.
2. **Starter and session lifecycle:** `src/beta_launcher_win.cpp` uses Win32
   process/environment/DLL APIs. `src/portable_language.h` hardcodes `.exe`
   siblings and its non-Windows handoff deliberately errors. The reset supervisor
   in `src/beta_session.h` is Windows-only and the reset control is gated in
   `src/main.cpp`. Implement and test native process launch, argument passing,
   exit/reset handling and JP/English routing together; simply dropping `.exe`
   or changing an error message would not implement this.
3. **Private playable build:** provide a native generation/build recipe for the
   verified Japanese ROM, English 1.1.0f ROM and BIOS. Regenerate for the target
   toolchain; do not reuse Windows objects. Preserve ROM verification, separate
   save banks and all current guarded game hooks.
4. **Native distribution:** the existing PowerShell build helper and PE/DLL ZIP
   packager are Windows-specific. Linux needs executable permissions and a
   validated shared-library/runtime policy. macOS needs a bundle/resource layout,
   dylib loading and signing/distribution decisions. Keep writable user data out
   of a signed/read-only application bundle. Decide explicit portable-data versus
   per-user data behavior before implementing it; never silently relocate saves.
5. **Native input, graphics and audio:** verify launcher file pickers (Linux
   zenity/kdialog and macOS native-dialog path), case-sensitive paths, non-ASCII
   filenames, moved folders, window controls, controllers/reconnect, fullscreen,
   all graphics filters and audio pacing. Linux X11/Wayland and macOS Retina/
   resize behavior need real-host checks, not dummy SDL video-driver results.
6. **Gameplay parity:** repeat both-language boot/save/load, Guard, Esc/pause/
   resume/reset, F10, all arenas/transitions and widescreen-state coverage using
   local private inputs. Audit architecture-sensitive runtime paths and interpreter
   fallback. Human visual/audio review and clean-machine launch remain separate
   acceptance gates; only then consider a labelled platform preview.

No merge, release publication, signing enrollment, new system installation or
tester contact is authorized by this preparation work.
