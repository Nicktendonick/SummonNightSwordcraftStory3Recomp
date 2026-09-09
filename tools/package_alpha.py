"""Package this game's pinned alpha snapshot for LOCAL review, never upload it.

Uses an explicit allowlist, strips only copied binaries, verifies their loaded
sections, inspects PE imports and tests an extracted archive with a minimal PATH.
Publication/provenance approval is deliberately not an option in this tool.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import sys
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE = "Alpha unfinished-build v.01"
TAG = "alpha-unfinished-build-v.01"
EXE = "SummonNightSwordcraftStory3RecompBeta.exe"
EXE_SHA256 = "0bafb1fc1e24ea41670dffb423e8274c1a8a25438787710ad6521641c176a190"
PINS = {
    "game": "d2cbeaa7eabafb6e5112ac21d47bedfc635b2955",
    "gbarecomp": "c9736473a9d27a6e932295c44ea3d2eb111b129b",
    "recomp-ui": "92fc0aea585a933e92db17d5079d6d09b6600613",
}
PACKAGE = "packaging/alpha-v01"
FILES = [(f"build-beta/{EXE}", EXE, "game / linked runtime", "distribution review OPEN")]
for name, component, license_name in [
    ("SDL2.dll", "SDL2 2.30.11-1", "zlib"),
    ("libgcc_s_seh-1.dll", "gcc-libs 14.2.0-2", "GPL-3.0-or-later WITH GCC-exception-3.1"),
    ("libstdc++-6.dll", "gcc-libs 14.2.0-2", "GPL-3.0-or-later WITH GCC-exception-3.1"),
    ("libwinpthread-1.dll", "libwinpthread 12.0.0.r473.gce0d0bfb7-1", "see COPYING"),
]:
    FILES.append((f"build-beta/{name}", name, component, license_name))
for name in ["LatoLatin-Regular.ttf", "LatoLatin-Bold.ttf", "OpenMoji-black-glyf.ttf", "NotoSansSymbols2-Regular.ttf"]:
    FILES.append((f"recomp-ui/assets/common/fonts/{name}", f"assets/fonts/{name}", "recomp-ui fonts", "full notice/provenance review OPEN"))
for name in ["brand_mark.tga", "verdict_ok.tga", "verdict_warn.tga", "verdict_bad.tga", "verdict_none.tga"]:
    FILES.append((f"recomp-ui/assets/common/img/{name}", f"assets/img/{name}", "recomp-ui images", "provenance review OPEN"))
FILES.append(("recomp-ui/assets/consoles/gba/img/pad_gba.tga", "assets/img/pad_gba.tga", "recomp-ui GBA image", "provenance review OPEN"))
for name in ["START-HERE.md", "RELEASE-NOTES.md", "LICENSE-REVIEW.md", "SMOKE-TEST.md", "Launch.cmd"]:
    FILES.append((f"{PACKAGE}/{name}", name, "game release preparation", "root license decision OPEN"))
for source, name, component in [
    ("gbarecomp/LICENSE", "gbarecomp-LICENSE.txt", "gbarecomp"),
    ("gbarecomp/THIRD_PARTY_ATTRIBUTION.md", "gbarecomp-THIRD_PARTY_ATTRIBUTION.md", "gbarecomp attribution"),
    ("recomp-ui/LICENSE", "recomp-ui-LICENSE.txt", "recomp-ui"),
    ("recomp-ui/src/third_party/imgui/LICENSE.txt", "imgui-LICENSE.txt", "Dear ImGui"),
    ("recomp-ui/assets/common/fonts/NOTICE.md", "fonts-NOTICE.md", "recomp-ui fonts"),
    ("build-beta/_deps/tomlplusplus-src/LICENSE", "tomlplusplus-LICENSE.txt", "toml++ v3.4.0"),
]:
    FILES.append((source, f"notices/{name}", component, "verbatim upstream notice"))
for name in ["SDL2-LICENSE.txt", "libwinpthread-COPYING.txt", "gcc-libs-README.txt", "gcc-libs-COPYING3.txt", "gcc-libs-COPYING.RUNTIME.txt"]:
    FILES.append((f"{PACKAGE}/notices/{name}", f"notices/{name}", "installed MSYS2 runtime notices", "verbatim installed notice"))

SYSTEM_DLLS = set("advapi32.dll bcrypt.dll cfgmgr32.dll comdlg32.dll dinput8.dll dwmapi.dll dxgi.dll gdi32.dll hid.dll imm32.dll kernel32.dll msvcrt.dll ntdll.dll ole32.dll oleaut32.dll opengl32.dll powrprof.dll rpcrt4.dll setupapi.dll shell32.dll shlwapi.dll user32.dll uxtheme.dll version.dll winmm.dll ws2_32.dll".split())
BLOCKERS = [
    "Compiled game / translation / BIOS distribution review is open.",
    "Root license / complete linked-code and asset notices review is open.",
    "Interactive clean-machine gameplay/audio/input/assist tests are not completed.",
    "Owner has not approved the final artifact list for publication.",
]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def safe_relative(value):
    # Reject Windows alternate streams, drives, backslashes and ambiguous paths
    # even when the verifier is run on another operating system.
    if not value or not re.fullmatch(r"[A-Za-z0-9_./ +-]+", value):
        raise ValueError(f"Unsafe relative path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(p in ("", ".", "..") or p.endswith((".", " ")) for p in value.split("/")):
        raise ValueError(f"Unsafe relative path: {value!r}")
    if any(p.split(".")[0].upper() in {"CON", "PRN", "AUX", "NUL", *[f"COM{i}" for i in range(1, 10)], *[f"LPT{i}" for i in range(1, 10)]} for p in path.parts):
        raise ValueError(f"Reserved Windows path: {value!r}")
    return path


def inside(base, relative):
    safe_relative(relative)
    target = base.joinpath(relative).resolve()
    if not target.is_relative_to(base.resolve()) or target == base.resolve():
        raise ValueError(f"Path escapes its allowed root: {relative}")
    return target


def pe_info(path):
    """Read PE32+ sections and static/delay import names without extra tools."""
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError(f"Not a PE file: {path.name}")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine, count, _, symbols, symbol_count, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
    opt = pe + 24
    if machine != 0x8664 or struct.unpack_from("<H", data, opt)[0] != 0x20B:
        raise ValueError("This package requires x64 PE32+ binaries")
    entry = struct.unpack_from("<I", data, opt + 16)[0]
    sections = []
    loaded = {}
    debug = []
    for i in range(count):
        row = opt + opt_size + i * 40
        name = data[row:row + 8].rstrip(b"\0").decode("ascii")
        if name.startswith("/"):
            start = symbols + symbol_count * 18 + int(name[1:])
            name = data[start:data.index(b"\0", start)].decode("ascii")
        virtual_size, rva, raw_size, offset = struct.unpack_from("<IIII", data, row + 8)
        flags = struct.unpack_from("<I", data, row + 36)[0]
        sections.append((rva, virtual_size, raw_size, offset))
        if name.startswith(".debug") or name in (".stab", ".stabstr"):
            debug.append(name)
        elif flags & 0xE0000000:  # memory-read/write/execute: retained loaded data
            meaningful = min(raw_size, virtual_size) if virtual_size else raw_size
            loaded[name] = {"rva": rva, "virtual_size": virtual_size, "sha256": sha256(data[offset:offset + meaningful])}

    def file_offset(rva):
        for start, size, raw_size, offset in sections:
            if start <= rva < start + max(size, raw_size):
                if rva - start >= raw_size:
                    raise ValueError("Import points outside initialized data")
                return offset + rva - start
        raise ValueError(f"Unmapped import RVA: {rva:x}")

    imports = set()
    directories = struct.unpack_from("<I", data, opt + 108)[0]
    for index, row_size, name_offset in [(1, 20, 12), (13, 32, 4)]:
        if directories <= index:
            continue
        table_rva, table_size = struct.unpack_from("<II", data, opt + 112 + index * 8)
        if not table_rva:
            continue
        row = file_offset(table_rva)
        for _ in range(table_size // row_size + 1):
            if not any(data[row:row + row_size]):
                break
            if index == 13 and not (struct.unpack_from("<I", data, row)[0] & 1):
                raise ValueError("Unsupported VA-form delay import")
            name_rva = struct.unpack_from("<I", data, row + name_offset)[0]
            start = file_offset(name_rva)
            imports.add(data[start:data.index(b"\0", start)].decode("ascii").lower())
            row += row_size
        else:
            raise ValueError("Unterminated import table")
    path_markers = [pattern for pattern in ("C:\\Users\\", "C:/Users/", "C:\\msys64\\", "C:/msys64/")
                    if pattern.encode() in data or pattern.encode("utf-16le") in data]
    return {"machine": "x64", "entry_rva": entry, "imports": sorted(imports), "loaded_sections": loaded, "debug_sections": debug, "path_markers": path_markers}


def check_dependencies(binaries):
    packaged = {name.lower() for name in binaries}
    unresolved = {}
    for name, info in binaries.items():
        missing = [dll for dll in info["imports"] if dll not in packaged and dll not in SYSTEM_DLLS
                   and not dll.startswith(("api-ms-win-", "ext-ms-win-"))]
        if missing:
            unresolved[name] = missing
    if unresolved:
        raise ValueError(f"Unresolved non-system PE imports: {unresolved}")


def verify_archive(archive, allowed):
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len({n.casefold() for n in names}) or set(names) != set(allowed):
            raise ValueError("Archive contains duplicate, extra or missing entries")
        for name in names:
            safe_relative(name)
            mode = (z.getinfo(name).external_attr >> 16) & 0o170000
            if mode == 0o120000 or z.getinfo(name).flag_bits & 1:
                raise ValueError("Symlinks/encrypted archive entries are not allowed")
        manifest = json.loads(z.read("MANIFEST.json"))
        rows = manifest["files"]
        if len(rows) != len({r["path"].casefold() for r in rows}) or {r["path"] for r in rows} != set(names) - {"MANIFEST.json", "SHA256SUMS.txt"}:
            raise ValueError("Manifest does not exactly match payload")
        for row in rows:
            content = z.read(row["path"])
            if len(content) != row["bytes"] or sha256(content) != row["sha256"]:
                raise ValueError(f"Payload hash mismatch: {row['path']}")
        lines = z.read("SHA256SUMS.txt").decode("utf-8").splitlines()
        checksums = [line.split("  ", 1) for line in lines]
        if len(checksums) != len(names) - 1 or {r[1] for r in checksums} != set(names) - {"SHA256SUMS.txt"}:
            raise ValueError("Checksum list does not exactly match archive")
        for digest, name in checksums:
            if sha256(z.read(name)) != digest:
                raise ValueError(f"Checksum mismatch: {name}")
    return manifest


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, errors="replace", timeout=90, **kwargs).stdout.strip()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def prepare(strip):
    # Lock the candidate to the known working binary, not whichever build happens
    # to exist later. Metadata/docs may be dirty, but implementation may not be.
    for component, pin in PINS.items():
        repo = ROOT if component == "game" else ROOT / component
        if run(["git", "-C", str(repo), "rev-parse", "HEAD"]) != pin:
            raise ValueError(f"Pinned revision changed for {component}; review this recipe first")
        args = ["git", "-C", str(repo), "status", "--porcelain", "--untracked-files=all"]
        if component == "game":
            args += ["--", "src", "CMakeLists.txt", "game.toml"]
        if run(args):
            raise ValueError(f"Uncommitted implementation changes in {component}")
    if file_hash(ROOT / "build-beta" / EXE) != EXE_SHA256:
        raise ValueError("The existing English-beta executable does not match the reviewed snapshot")
    if not strip.is_file():
        raise ValueError("MinGW strip.exe is required to strip copies, never the working binaries")
    destinations = [row[1] for row in FILES]
    if len(destinations) != len({name.casefold() for name in destinations}):
        raise ValueError("Duplicate allowlist destination")
    for source, dest, _, _ in FILES:
        safe_relative(dest)
        if not inside(ROOT, source).is_file():
            raise ValueError(f"Missing allowlisted input: {source}")

    stage_root = inside(ROOT, f"release-stage/{TAG}")
    stage_root.mkdir(parents=True, exist_ok=True)
    candidate = stage_root / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8])
    candidate.mkdir(exist_ok=False)
    payload = candidate / "payload"
    payload.mkdir()
    print(f"Local candidate: {candidate}", flush=True)
    rows, binaries = [], {}
    strip_env = os.environ.copy()
    strip_env["PATH"] = str(strip.parent) + os.pathsep + strip_env.get("PATH", "")
    for source, dest, component, license_name in FILES:
        src = inside(ROOT, source)
        dst = inside(payload, dest)
        dst.parent.mkdir(parents=True, exist_ok=True)
        source_hash = file_hash(src)
        if dest.startswith("assets/") and source_hash != file_hash(ROOT / "build-beta" / dest):
            raise ValueError(f"Source asset differs from the tested build: {dest}")
        if dst.suffix == ".dll" and source_hash != file_hash(strip.parent / dst.name):
            raise ValueError(f"Installed runtime differs; re-review version/notices: {dest}")
        shutil.copyfile(src, dst)
        if dst.suffix in (".exe", ".dll"):
            before = pe_info(src)
            run([str(strip), "--strip-debug", str(dst)], env=strip_env)
            after = pe_info(dst)
            for key in ("loaded_sections", "imports", "entry_rva"):
                if before[key] != after[key]:
                    raise ValueError(f"Stripping changed {key}: {dest}")
            if after["debug_sections"]:
                raise ValueError(f"Debug sections remain: {dest}")
            binaries[dest] = after
        if file_hash(src) != source_hash:
            raise ValueError(f"Source changed during packaging: {source}")
        rows.append({"path": dest, "source": source, "component": component, "license_status": license_name,
                     "source_sha256": source_hash, "bytes": dst.stat().st_size, "sha256": file_hash(dst),
                     "transformation": "strip-debug; loaded sections verified unchanged" if dst.suffix in (".exe", ".dll") else "verbatim copy"})
    check_dependencies(binaries)
    manifest = {"schema": 1, "title": TITLE, "proposed_tag": TAG, "variant": "English-beta",
                "publication_ready": False, "publication_blockers": BLOCKERS, "revisions": PINS,
                "build_identity_basis": "Pinned existing executable hash; no rebuild or new gameplay changes",
                "local_preparation_changes": run(["git", "status", "--short"], cwd=ROOT).splitlines(),
                "pe_imports": {name: info["imports"] for name, info in binaries.items()}, "files": rows}
    write_json(payload / "MANIFEST.json", manifest)
    allowed = set(destinations) | {"MANIFEST.json", "SHA256SUMS.txt"}
    checksum_names = sorted(allowed - {"SHA256SUMS.txt"})
    (payload / "SHA256SUMS.txt").write_text("".join(f"{file_hash(payload / name)}  {name}\n" for name in checksum_names), encoding="utf-8")
    actual = {p.relative_to(payload).as_posix() for p in payload.rglob("*") if p.is_file()}
    if actual != allowed:
        raise ValueError("Unexpected files in staging")
    archive = candidate / f"SummonNightSwordcraftStory3Recomp-{TAG}-Windows-x64-LOCAL-REVIEW.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name in sorted(allowed):
            z.write(payload / name, name)
    verify_archive(archive, allowed)
    archive_hash = file_hash(archive)
    archive.with_suffix(".zip.sha256").write_text(f"{archive_hash}  {archive.name}\n", encoding="utf-8")

    # Validate exact ZIP, not loose build files. Test outputs stay OUTSIDE the ZIP.
    extracted = candidate / "smoke extracted with spaces"
    extracted.mkdir()
    with zipfile.ZipFile(archive) as z:
        z.extractall(extracted)  # exact allowlist and safe paths already verified
    windows = os.environ.get("SystemRoot", r"C:\Windows")
    test_env = {k: v for k, v in os.environ.items() if not k.upper().startswith(("GBARECOMP_", "SWORDCRAFT3_", "SDL_"))}
    test_env["PATH"] = os.pathsep.join([str(Path(windows) / "System32"), windows])
    test_env["SDL_VIDEODRIVER"] = "dummy"
    test_env["SDL_AUDIODRIVER"] = "dummy"
    smoke = []
    for label, args, expected in [
        ("help_without_developer_path", ["--help"], 0),
        ("invalid_argument_rejected", ["--no-launcher", "--no-window", "--frames", "not-a-number"], 1),
    ]:
        try:
            result = subprocess.run([str(extracted / EXE), *args], cwd=extracted, env=test_env,
                                    capture_output=True, text=True, errors="replace", timeout=30)
            marker = "A legally dumped" if expected == 0 else "invalid --frames value"
            smoke.append({"name": label, "passed": result.returncode == expected and marker in result.stdout + result.stderr,
                          "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
        except subprocess.TimeoutExpired:
            smoke.append({"name": label, "passed": False, "error": "Timed out after 30 seconds; test process stopped"})
    verify_archive(archive, allowed)
    if file_hash(archive) != archive_hash:
        raise ValueError("Archive changed during smoke testing")
    write_json(candidate / "LOCAL-REVIEW-REPORT.json", {
        "title": TITLE, "archive": archive.name, "archive_bytes": archive.stat().st_size,
        "archive_sha256": archive_hash, "payload_files": len(allowed),
        "source_binaries_unchanged": True, "archive_allowlist_and_hashes_passed": True,
        "loaded_sections_unchanged_after_stripping": True, "static_and_delay_import_closure_passed": True,
        "binary_audit": binaries, "smoke_tests": smoke, "clean_machine_test": "NOT RUN",
        "missing_input_test": "MANUAL: Windows asset picker opens even with --no-window. No noninteractive rejection claim.",
        "dynamic_dependencies": "Driver/OS LoadLibrary paths and self-healing compiler/cache routes require interactive review",
        "publication_ready": False, "publication_blockers": BLOCKERS,
    })
    (candidate / "FILE-LIST.txt").write_text("\n".join(f"{(payload / name).stat().st_size:>10}  {file_hash(payload / name)}  {name}" for name in sorted(allowed)) + "\n", encoding="utf-8")
    print(f"Title: {TITLE}\nFiles: {len(allowed)}\nZIP: {archive}\nSHA256: {archive_hash}")
    print("Local technical smoke checks: " + ("PASS" if all(t["passed"] for t in smoke) else "FAIL"))
    print("NOT approved for publication. Read LOCAL-REVIEW-REPORT.json and LICENSE-REVIEW.md.")
    if not all(t["passed"] for t in smoke):
        raise ValueError("Extracted package smoke check failed; candidate retained for inspection")
    return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strip", type=Path, default=Path(r"C:\msys64\mingw64\bin\strip.exe"))
    args = parser.parse_args()
    try:
        prepare(args.strip.resolve())
    except (ValueError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as exc:
        print(f"Packaging stopped: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
