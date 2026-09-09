import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_alpha", ROOT / "tools/package_alpha.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class AlphaPackageTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / "release-stage/package-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def archive(self, extras=None, bad_hash=False):
        body = b"local release test\n"
        rows = [{"path": "readme.txt", "bytes": len(body), "sha256": "bad" if bad_hash else PACKAGE.sha256(body)}]
        files = {"readme.txt": body, "MANIFEST.json": json.dumps({"files": rows}).encode()}
        files["SHA256SUMS.txt"] = "".join(f"{PACKAGE.sha256(data)}  {name}\n" for name, data in files.items()).encode()
        if extras:
            files.update(extras)
        path = self.root / "test.zip"
        with zipfile.ZipFile(path, "w") as z:
            for name, body in files.items():
                z.writestr(name, body)
        return path

    def test_safe_paths(self):
        for value in ["../rom.gba", "/rom.gba", "C:/rom.gba", "a\\b", "a/../b", "a//b", "a/./b", "foo:bar", "CON.txt", "aux/x", "a. /b"]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                PACKAGE.safe_relative(value)
        self.assertEqual(str(PACKAGE.safe_relative("assets/fonts/example.ttf")), "assets/fonts/example.ttf")

    def test_inside_rejects_escape(self):
        with self.assertRaises(ValueError):
            PACKAGE.inside(self.root, "../private.gba")

    def test_exact_allowlist_unique(self):
        destinations = [row[1].casefold() for row in PACKAGE.FILES]
        self.assertEqual(len(destinations), len(set(destinations)))
        for source, dest, _, _ in PACKAGE.FILES:
            PACKAGE.safe_relative(source)
            PACKAGE.safe_relative(dest)
            self.assertNotIn(Path(dest).suffix.lower(), {".gba", ".bin", ".sav", ".bps", ".ips", ".ips32", ".o", ".obj", ".cpp", ".ini", ".cfg", ".mp4", ".png"})

    def test_valid_archive(self):
        PACKAGE.verify_archive(self.archive(), {"readme.txt", "MANIFEST.json", "SHA256SUMS.txt"})

    def test_extra_rom_rejected(self):
        with self.assertRaises(ValueError):
            PACKAGE.verify_archive(self.archive({"secret.gba": b"not allowed"}), {"readme.txt", "MANIFEST.json", "SHA256SUMS.txt"})

    def test_traversal_archive_rejected(self):
        path = self.archive({"../rom.gba": b"not allowed"})
        with self.assertRaises(ValueError):
            PACKAGE.verify_archive(path, {"readme.txt", "MANIFEST.json", "SHA256SUMS.txt", "../rom.gba"})

    def test_hash_tamper_rejected(self):
        with self.assertRaises(ValueError):
            PACKAGE.verify_archive(self.archive(bad_hash=True), {"readme.txt", "MANIFEST.json", "SHA256SUMS.txt"})

    def test_case_collision_rejected(self):
        path = self.archive({"README.TXT": b"duplicate on Windows"})
        with self.assertRaises(ValueError):
            PACKAGE.verify_archive(path, {"readme.txt", "README.TXT", "MANIFEST.json", "SHA256SUMS.txt"})

    def test_missing_non_system_dependency_rejected(self):
        with self.assertRaises(ValueError):
            PACKAGE.check_dependencies({"game.exe": {"imports": ["missing.dll"]}})
        PACKAGE.check_dependencies({"game.exe": {"imports": ["kernel32.dll", "sdl2.dll"]}, "SDL2.dll": {"imports": ["user32.dll"]}})

    def test_pe_sections_and_imports(self):
        data = bytearray(2048)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 0x80)
        data[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", data, 0x84, 0x8664, 1, 0, 0, 0, 240, 0)
        opt = 0x98
        struct.pack_into("<H", data, opt, 0x20B)
        struct.pack_into("<I", data, opt + 16, 0x1000)
        struct.pack_into("<I", data, opt + 108, 16)
        struct.pack_into("<II", data, opt + 120, 0x1000, 40)
        row = opt + 240
        data[row:row + 8] = b".idata\0\0"
        struct.pack_into("<IIII", data, row + 8, 0x200, 0x1000, 0x200, 0x400)
        struct.pack_into("<I", data, row + 36, 0x40000040)
        struct.pack_into("<I", data, 0x400 + 12, 0x1080)
        data[0x480:0x48D] = b"KERNEL32.dll\0"
        path = self.root / "test.exe"
        path.write_bytes(data)
        info = PACKAGE.pe_info(path)
        self.assertEqual(info["imports"], ["kernel32.dll"])
        self.assertEqual(info["entry_rva"], 0x1000)
        self.assertEqual(info["loaded_sections"][".idata"]["sha256"], PACKAGE.sha256(data[0x400:0x600]))


if __name__ == "__main__":
    unittest.main()
