#!/bin/sh
# Native Linux/macOS source checks only: no game inputs or playable package.
# Configure CMake through options; never rewrite CMakeLists.txt in-place.
set -eu

sc3_usage() {
    printf '%s\n' \
        'Usage: sh tools/check_platform.sh [--sdl-prefix PATH] [--build-dir PATH] [--jobs N]' \
        '' \
        'Runs ROM-free game/link and standalone launcher checks on Linux/macOS.' \
        'Requires a recursive clone, C++20 toolchain, CMake >=3.24, Ninja,' \
        'Python >=3.11, SDL2 and desktop OpenGL development files.' \
        'Dependencies are not installed automatically. No source files are rewritten.' \
        'The build directory defaults to <repo>/build-platform; UI uses its -ui sibling.' \
        'Relative option paths are relative to the caller working directory.' \
        'Do not use this test executable as a playable game.'
}

sc3_source=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
sc3_build="$sc3_source/build-platform"
sc3_sdl_prefix=''
sc3_jobs=2
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h) sc3_usage; exit 0 ;;
        --sdl-prefix|--build-dir|--jobs)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                printf 'Missing value for %s\n' "$1" >&2; exit 2
            fi
            case "$1" in
                --sdl-prefix) sc3_sdl_prefix=$2 ;;
                --build-dir) sc3_build=$2 ;;
                --jobs) sc3_jobs=$2 ;;
            esac
            shift 2 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; sc3_usage >&2; exit 2 ;;
    esac
done
case "$sc3_jobs" in
    ''|0|*[!0-9]*) printf '%s\n' '--jobs must be a positive integer.' >&2; exit 2 ;;
esac
case "$(uname -s)" in
    Linux|Darwin) ;;
    *) printf '%s\n' 'Use the Windows workflow on Windows; this script needs native Linux or macOS.' >&2; exit 2 ;;
esac
for sc3_tool in cmake ctest ninja python3; do
    command -v "$sc3_tool" >/dev/null 2>&1 || {
        printf 'Required tool is missing: %s\n' "$sc3_tool" >&2; exit 2;
    }
done
for sc3_dependency in gbarecomp/CMakeLists.txt recomp-ui/recomp_ui.cmake; do
    if [ ! -f "$sc3_source/$sc3_dependency" ]; then
        printf '%s\n' 'Initialize the pinned submodules: git submodule update --init --recursive' >&2
        exit 2
    fi
done
if [ -n "$sc3_sdl_prefix" ] && [ ! -d "$sc3_sdl_prefix" ]; then
    printf 'SDL2 prefix does not exist: %s\n' "$sc3_sdl_prefix" >&2; exit 2
fi

uname -a
cmake --version
python3 --version
sc3_python=$(command -v python3)

# Keep the default system search when no prefix is requested. A caller-supplied
# prefix reaches BOTH SDL discovery paths (runtime and launcher), not SDL2_DIR
# alone. Generator/compiler selection belongs to a dedicated build directory.
set -- -S "$sc3_source" -B "$sc3_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGBARECOMP_COMPILER_CACHE=OFF \
    "-DPython3_EXECUTABLE=$sc3_python"
if [ -n "$sc3_sdl_prefix" ]; then set -- "$@" "-DCMAKE_PREFIX_PATH=$sc3_sdl_prefix"; fi
cmake "$@"
cmake --build "$sc3_build" --target swordcraft3_platform_checks --parallel "$sc3_jobs"
ctest --test-dir "$sc3_build" -L '^swordcraft3-(rom-free|platform)$' \
    --output-on-failure --no-tests=error --timeout 120

set -- -S "$sc3_source/recomp-ui" -B "$sc3_build-ui" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSNESRECOMP_SDL_BACKEND=SDL2 -DBUILD_TESTING=ON
if [ -n "$sc3_sdl_prefix" ]; then set -- "$@" "-DCMAKE_PREFIX_PATH=$sc3_sdl_prefix"; fi
cmake "$@"
cmake --build "$sc3_build-ui" --target recomp-ui-scripted-key-test \
    recomp-ui-capture-hotplug-test --parallel "$sc3_jobs"
ctest --test-dir "$sc3_build-ui" -R '^recomp-ui-(scripted-key|capture-hotplug)$' \
    --output-on-failure --no-tests=error --timeout 120
printf '%s\n' 'PASS: source build/link and synthetic checks. Playable Linux/macOS support remains unverified.'
