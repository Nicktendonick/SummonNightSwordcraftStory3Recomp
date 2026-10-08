"""ROM-free tests for the strict portable ZIP verifier."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_portable_release as package


class PortableArchiveTests(unittest.TestCase):
    def setUp(self):
        # All temporary evidence stays in this project's ignored validation tree.
        base = ROOT / "validation"
        base.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="portable-zip-test-", dir=base)
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / "synthetic.zip"
        self.files = {package.STARTER: b"synthetic launcher, not an executable", "README.md": b"Test"}
        self.recipe = patch.object(package, "recipe", return_value=[(ROOT / k, k) for k in self.files])
        self.recipe.start()
        self.addCleanup(self.recipe.stop)

    def make_zip(self, change=None, extra=None, omit_dir=None):
        def digest(data):
            return hashlib.sha256(data).hexdigest()
        files = dict(self.files)
        manifest = {"files": [{"path": k, "bytes": len(v), "sha256": digest(v)} for k, v in files.items()]}
        files[package.MANIFEST] = json.dumps(manifest).encode()
        files[package.SUMS] = "".join(f"{digest(v)}  {k}\n" for k, v in files.items()).encode()
        if change:
            files[change] += b"tampered"
        if extra:
            files[extra] = b"unexpected private file"
        with zipfile.ZipFile(self.archive, "x") as z:
            z.mkdir(package.FOLDER + "/")
            for directory in package.DATA_DIRS:
                if directory != omit_dir:
                    z.mkdir(f"{package.FOLDER}/{directory}/")
            for name, data in files.items():
                z.writestr(f"{package.FOLDER}/{name}", data)

    def test_clean_layout(self):
        self.make_zip()
        self.assertEqual(len(package.verify_archive(self.archive)["files"]), 2)

    def test_changed_payload_rejected(self):
        self.make_zip(change=package.STARTER)
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            package.verify_archive(self.archive)

    def test_private_input_rejected(self):
        self.make_zip(extra="BIOS/private.bin")
        with self.assertRaisesRegex(ValueError, "allowlist"):
            package.verify_archive(self.archive)

    def test_player_settings_rejected(self):
        self.make_zip(extra="Settings/launcher.ini")
        with self.assertRaisesRegex(ValueError, "allowlist"):
            package.verify_archive(self.archive)

    def test_missing_empty_folder_rejected(self):
        self.make_zip(omit_dir="Saves")
        with self.assertRaisesRegex(ValueError, "allowlist"):
            package.verify_archive(self.archive)

    def test_reviewed_sdl_does_not_require_system_sdl_update(self):
        pin = json.loads((ROOT / "packaging/sdl2.json").read_text())
        with patch.object(package, "file_hash", return_value=pin["dll_sha256"]) as hashes:
            package.verify_runtime_dll(ROOT / "SDL2.dll", ROOT / "unused-system")
            hashes.assert_called_once_with(ROOT / "SDL2.dll")

    def test_old_or_tampered_sdl_rejected(self):
        with patch.object(package, "file_hash", return_value="wrong"):
            with self.assertRaisesRegex(ValueError, "SDL2 does not match"):
                package.verify_runtime_dll(ROOT / "SDL2.dll", ROOT)

    def test_other_runtime_dlls_still_match_compiler(self):
        with patch.object(package, "file_hash", side_effect=["one", "two"]):
            with self.assertRaisesRegex(ValueError, "DLL differs"):
                package.verify_runtime_dll(ROOT / "libstdc++-6.dll", ROOT)


if __name__ == "__main__":
    unittest.main()
