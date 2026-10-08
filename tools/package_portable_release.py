"""Prepare a clean portable candidate, never zip a played install or upload it.

Reuses the alpha packager's PE/import auditing, not its obsolete layout or pins.
Outputs stay under the owner project. Unknown files are never swept into the ZIP.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import zipfile

from package_alpha import pe_info, check_dependencies, safe_relative, file_hash

ROOT = Path(__file__).resolve().parents[1]
OWNER = ROOT.parents[1]
BUILD = ROOT / "build-native"
STARTER = "Swordcraft Story 3 Beta.exe"
GAME = "Swordcraft3CustomRendererBeta.exe"
JAPANESE = "Swordcraft3Japanese.exe"
FOLDER = "Swordcraft Story 3 Portable Beta"
DATA_DIRS = ["BIOS", "Captures", "Logs", "Mods", "ROMs", "Save States", "Saves", "Settings"]
MANIFEST = "Runtime/package-manifest.json"
SUMS = "Runtime/SHA256SUMS.txt"
GIT = Path("C:/Program Files/Git/cmd/git.exe")


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True,
                          errors="replace", timeout=90, **kwargs).stdout.strip()


def recipe():
    files = [(BUILD / STARTER, STARTER),
             (BUILD / "Swordcraft3Translation106.exe", "Runtime/" + GAME),
             (BUILD / "Swordcraft3Japanese106.exe", "Runtime/" + JAPANESE),
             (ROOT / "native-test.toml", "Runtime/game.toml")]
    for name in ["SDL2.dll", "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"]:
        files.append((BUILD / name, "Runtime/" + name))
    for name in ["boxart-clean", "boxart-original", "art-archer", "art-winter",
                 "art-parchment", "art-crystal", "art-glasses", "art-ensemble",
                 "art-horizon", "art-beginnings", "art-swordsmith", "launcher-logo", "launcher-village"]:
        files.append((ROOT / f"assets/beta/{name}.png", f"Runtime/assets/beta/{name}.png"))
    for name in ["LatoLatin-Regular.ttf", "LatoLatin-Bold.ttf", "OpenMoji-black-glyf.ttf", "NotoSansSymbols2-Regular.ttf"]:
        files.append((ROOT / f"recomp-ui/assets/common/fonts/{name}", f"Runtime/assets/fonts/{name}"))
    for name in ["NotoSansJP-Regular.ttf", "NotoSansJP-OFL.txt"]:
        files.append((ROOT / f"assets/fonts/{name}", f"Runtime/assets/fonts/{name}"))
    for name in ["brand_mark", "verdict_ok", "verdict_warn", "verdict_bad", "verdict_none"]:
        files.append((ROOT / f"recomp-ui/assets/common/img/{name}.tga", f"Runtime/assets/img/{name}.tga"))
    files.append((ROOT / "recomp-ui/assets/consoles/gba/img/pad_gba.tga", "Runtime/assets/img/pad_gba.tga"))
    for name in ["original-game.txt", "pc-port.txt", "tools-and-projects.txt"]:
        files.append((ROOT / f"assets/credits/{name}", "Credits/" + name))
    files.append((ROOT / "packaging/portable-beta/README.md", "README.md"))
    notices = [
        (ROOT / "packaging/portable-beta/RELEASE-REVIEW.md", "RELEASE-REVIEW.md"),
        (ROOT / "assets/beta/ARTWORK.md", "ARTWORK.md"),
        (ROOT / "gbarecomp/LICENSE", "gbarecomp-LICENSE.txt"),
        (ROOT / "gbarecomp/THIRD_PARTY_ATTRIBUTION.md", "gbarecomp-THIRD_PARTY_ATTRIBUTION.md"),
        (ROOT / "recomp-ui/LICENSE", "recomp-ui-LICENSE.txt"),
        (ROOT / "recomp-ui/src/third_party/imgui/LICENSE.txt", "imgui-LICENSE.txt"),
        (ROOT / "recomp-ui/assets/common/fonts/NOTICE.md", "fonts-NOTICE.md"),
        (ROOT / "assets/fonts/NotoSansJP-OFL.txt", "NotoSansJP-OFL.txt"),
    ]
    # Resolve the actual linked toml++ source from CMake, not an older build.
    cache = (BUILD / "CMakeCache.txt").read_text(encoding="utf-8")
    sources = [line.split("=", 1)[1] for line in cache.splitlines()
               if line.startswith("tomlplusplus_SOURCE_DIR:STATIC=")]
    if len(sources) != 1:
        raise ValueError("Cannot locate the linked toml++ license")
    toml = Path(sources[0]).resolve()
    if not toml.is_relative_to(OWNER):
        raise ValueError("Unexpected external toml++ source")
    notices.append((toml / "LICENSE", "tomlplusplus-LICENSE.txt"))
    notices.append((ROOT / "packaging/portable-beta/notices/SDL2-LICENSE.txt", "SDL2-LICENSE.txt"))
    for name in ["libwinpthread-COPYING.txt", "gcc-libs-README.txt",
                 "gcc-libs-COPYING3.txt", "gcc-libs-COPYING.RUNTIME.txt"]:
        notices.append((ROOT / "packaging/alpha-v01/notices" / name, name))
    files += [(source, "Runtime/notices/" + dest) for source, dest in notices]
    return files


def verify_archive(archive):
    expected = {dest for _, dest in recipe()} | {MANIFEST, SUMS}
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len({name.casefold() for name in names}):
            raise ValueError("Duplicate archive names")
        required_dirs = {FOLDER + "/"} | {FOLDER + "/" + d + "/" for d in DATA_DIRS}
        entries = {FOLDER + "/" + p for p in expected}
        if set(names) != entries | required_dirs:
            raise ValueError("ZIP does not exactly match the portable allowlist")
        for item in z.infolist():
            safe_relative(item.filename.rstrip("/"))
            if ((item.external_attr >> 16) & 0o170000) == 0o120000 or item.flag_bits & 1:
                raise ValueError("Symlinks/encrypted entries are not allowed")
        manifest = json.loads(z.read(FOLDER + "/" + MANIFEST))
        rows = manifest["files"]
        if len(rows) != len(expected) - 2 or {r["path"] for r in rows} != expected - {MANIFEST, SUMS}:
            raise ValueError("Manifest payload mismatch")
        for row in rows:
            data = z.read(FOLDER + "/" + row["path"])
            if len(data) != row["bytes"] or hashlib.sha256(data).hexdigest() != row["sha256"]:
                raise ValueError("Payload checksum mismatch: " + row["path"])
        checks = [line.split("  ", 1) for line in z.read(FOLDER + "/" + SUMS).decode().splitlines()]
        if len(checks) != len(expected) - 1 or {p for _, p in checks} != expected - {SUMS}:
            raise ValueError("Checksum list mismatch")
        for digest, name in checks:
            if hashlib.sha256(z.read(FOLDER + "/" + name)).hexdigest() != digest:
                raise ValueError("Checksum mismatch: " + name)
    return manifest


def verify_runtime_dll(source, compiler_bin):
    if source.name == "SDL2.dll":
        pin = json.loads((ROOT / "packaging/sdl2.json").read_text())
        if file_hash(source) != pin["dll_sha256"]:
            raise ValueError("SDL2 does not match the reviewed SDK pin")
    elif file_hash(source) != file_hash(compiler_bin / source.name):
        raise ValueError("DLL differs from installed runtime; re-review notices: " + source.name)


def prepare(strip):
    files = recipe()
    if len(files) != len({dest.casefold() for _, dest in files}):
        raise ValueError("Duplicate destinations")
    for source, dest in files:
        safe_relative(dest)
        if not source.is_file() or source.is_symlink():
            raise ValueError("Missing/linked input: " + str(source))
        if dest.startswith("Runtime/assets/") and file_hash(source) != file_hash(BUILD / dest[8:]):
            raise ValueError("Source asset does not match the tested build: " + dest)
        if source.suffix == ".dll":
            verify_runtime_dll(source, strip.parent)
    if not strip.is_file():
        raise ValueError("Missing debug stripping tool")
    # The config must contain only the non-private save type/size settings.
    import tomllib
    if tomllib.loads((ROOT / "native-test.toml").read_text()) != {"save": {"type": "eeprom", "size": "0x2000"}}:
        raise ValueError("Unexpected package configuration")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    candidate = OWNER / "release/Portable Release Candidates" / stamp
    payload = candidate / FOLDER
    payload.mkdir(parents=True, exist_ok=False)
    for name in DATA_DIRS:
        (payload / name).mkdir()
    print("Preparing " + str(candidate), flush=True)
    env = os.environ.copy()
    env["PATH"] = str(strip.parent) + os.pathsep + env.get("PATH", "")
    rows, binaries = [], {}
    for source, dest in files:
        target = payload / dest
        target.parent.mkdir(parents=True, exist_ok=True)
        original = file_hash(source)
        shutil.copyfile(source, target)
        if target.suffix in (".exe", ".dll"):
            before = pe_info(source)
            run([str(strip), "--strip-debug", str(target)], env=env)
            after = pe_info(target)
            for field in ("loaded_sections", "imports", "entry_rva"):
                if before[field] != after[field]:
                    raise ValueError("Stripping changed " + dest + " " + field)
            if after["debug_sections"]:
                raise ValueError("Debug sections remain")
            binaries[target.name] = after
        if file_hash(source) != original:
            raise ValueError("Packaging source changed: " + str(source))
        rows.append({"path": dest, "source": source.relative_to(OWNER).as_posix(),
                     "source_sha256": original, "sha256": file_hash(target), "bytes": target.stat().st_size})
    check_dependencies(binaries)
    revisions = {}
    for name, directory in [("game", ROOT), ("engine", ROOT / "gbarecomp"), ("ui", ROOT / "recomp-ui")]:
        diff = run([str(GIT), "-C", str(directory), "diff", "HEAD", "--", "."])
        revisions[name] = {"head": run([str(GIT), "-C", str(directory), "rev-parse", "HEAD"]),
                           "working_diff_sha256": hashlib.sha256(diff.encode()).hexdigest(),
                           "status": run([str(GIT), "-C", str(directory), "status", "--short"]).splitlines()}
    manifest = {"schema": 1, "created_utc": stamp, "variant": "Japanese + optional English 1.0.6.f",
                "publication_ready": False, "review": "Runtime/notices/RELEASE-REVIEW.md",
                "description": "Dual-engine portable UI test; organized launcher/Esc Graphics; English Translation in Mods; released English 1.0.6.f; optional battle framing; isolated revision saves",
                "revisions": revisions, "files": rows}
    (payload / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    allowed = {dest for _, dest in files} | {MANIFEST, SUMS}
    (payload / SUMS).write_text("".join(f"{file_hash(payload / name)}  {name}\n"
                                       for name in sorted(allowed - {SUMS})), encoding="utf-8")
    actual = {p.relative_to(payload).as_posix() for p in payload.rglob("*") if p.is_file()}
    if actual != allowed:
        raise ValueError("Unexpected staging file")
    archive = candidate / "Swordcraft-Story-3-Portable-Beta-Windows-x64-CANDIDATE.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        z.mkdir(FOLDER + "/")
        for name in DATA_DIRS:
            z.mkdir(FOLDER + "/" + name + "/")
        for name in sorted(allowed):
            z.write(payload / name, FOLDER + "/" + name)
    verify_archive(archive)
    digest = file_hash(archive)
    archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    # Test exactly these bytes in a separate disposable tree, never in payload.
    # A deep timestamped test path plus the verified patch-cache filename can
    # exceed the Windows C file API limit. Relocatability includes short paths.
    extracted = ROOT / "validation" / ("pkg-" + stamp[-8:])
    extracted.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(archive) as z:
        z.extractall(extracted)  # exact validated paths and entry list above
    report = {"archive": str(archive), "archive_sha256": digest,
              "archive_bytes": archive.stat().st_size, "payload_files": len(allowed),
              "file_allowlist_passed": True, "empty_player_data_folders": DATA_DIRS,
              "unchanged_loaded_sections_after_stripping": True, "dependency_closure_passed": True,
              "binary_audit": binaries, "test_package": str(extracted / FOLDER),
              "publication_ready": False}
    (candidate / "PACKAGE-REPORT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("archive", "archive_sha256", "archive_bytes", "payload_files", "test_package")}, indent=2))
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strip", type=Path, default=Path("C:/msys64/mingw64/bin/strip.exe"))
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args()
    try:
        if args.verify:
            verify_archive(args.verify)
            print("PASS: exact file allowlist, empty data directories, manifest and checksums")
        else:
            prepare(args.strip.resolve())
    except (ValueError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as exc:
        print("Packaging stopped: " + str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
