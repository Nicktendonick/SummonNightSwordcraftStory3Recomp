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
        manifest = {"schema": 2, "target": package.TARGET,
                    "files": [{"path": k, "bytes": len(v), "sha256": digest(v)} for k, v in files.items()]}
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

    def test_real_allowlist_verifies_without_developer_cache(self):
        self.recipe.stop()
        with patch.object(package, "BUILD", Path(self.temp.name) / "no-build"), \
                patch.object(package, "cmake_cache", side_effect=AssertionError("must not read build cache")):
            self.files = {dest: b"synthetic payload" for _, dest in package.recipe(resolve_build=False)}
            self.make_zip()
            self.assertEqual(len(package.verify_archive(self.archive)["files"]), len(self.files))

    def test_current_recipe_uses_110_engines(self):
        self.recipe.stop()
        files = {dest: source.name for source, dest in package.recipe(resolve_build=False)}
        self.assertEqual(files["Runtime/" + package.GAME], "Swordcraft3Translation110.exe")
        self.assertEqual(files["Runtime/" + package.JAPANESE], "Swordcraft3Japanese110.exe")
        self.assertEqual(package.TARGET["english_revision"], "1.1.0f")

    def test_wrong_translation_metadata_rejected(self):
        # Write a self-consistent archive for an older recipe, then verify it
        # with the current recipe. Valid hashes must not imply the right version.
        previous = dict(package.TARGET, english_revision="1.0.6f")
        with patch.object(package, "TARGET", previous):
            self.make_zip()
        with self.assertRaisesRegex(ValueError, "target/version"):
            package.verify_archive(self.archive)


class BuildProvenanceTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / "validation"
        base.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="build-record-test-", dir=base)
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name)
        self.build_patch = patch.object(package, "BUILD", self.build)
        self.build_patch.start()
        self.addCleanup(self.build_patch.stop)
        self.revisions = {"game": "a" * 40, "engine": "b" * 40, "ui": "c" * 40}
        self.git_patch = patch.object(package, "source_revisions", return_value=self.revisions)
        self.git_patch.start()
        self.addCleanup(self.git_patch.stop)
        names = [package.STARTER, package.TARGET["english_target"] + ".exe",
                 package.TARGET["japanese_target"] + ".exe", "SDL2.dll", "libstdc++-6.dll",
                 "libgcc_s_seh-1.dll", "libwinpthread-1.dll"]
        hashes = {}
        for name in names:
            file = self.build / name
            file.write_bytes(b"synthetic build artifact, not executable")
            hashes[name] = package.file_hash(file)
        self.record = {"schema": 1, "target": package.TARGET, "revisions": self.revisions,
                       "build_type": "Release", "generated_bios": True, "artifacts": hashes}
        self.save_record()
        self.cache = {"CMAKE_HOME_DIRECTORY": str(ROOT), "CMAKE_BUILD_TYPE": "Release",
                      "SC3_LAB_GUEST_OBJECT_DIR": "", package.TARGET["cmake_rom_option"]: "private-input.gba"}
        self.cache_patch = patch.object(package, "cmake_cache", return_value=self.cache)
        self.cache_patch.start()
        self.addCleanup(self.cache_patch.stop)

    def save_record(self):
        (self.build / "portable-build.json").write_text(json.dumps(self.record), encoding="utf-8-sig")

    def test_matching_record(self):
        self.assertEqual(package.build_record(), self.record)

    def test_changed_source_revision_rejected(self):
        self.record["revisions"] = dict(self.revisions, game="d" * 40)
        self.save_record()
        with self.assertRaisesRegex(ValueError, "revision mismatch"):
            package.build_record()

    def test_changed_executable_rejected(self):
        (self.build / (package.TARGET["english_target"] + ".exe")).write_bytes(b"different build")
        with self.assertRaisesRegex(ValueError, "artifact checksum"):
            package.build_record()

    def test_placeholder_bios_rejected(self):
        self.record["generated_bios"] = False
        self.save_record()
        with self.assertRaisesRegex(ValueError, "clean portable target"):
            package.build_record()

    def test_old_recipe_record_rejected(self):
        self.record["target"] = dict(package.TARGET, english_target="Swordcraft3Translation106")
        self.save_record()
        with self.assertRaisesRegex(ValueError, "clean portable target"):
            package.build_record()

    def test_mixed_guest_cache_rejected(self):
        self.cache["SC3_LAB_GUEST_OBJECT_DIR"] = "old-beta-objects"
        with self.assertRaisesRegex(ValueError, "Cached guest objects"):
            package.build_record()

    def test_other_checkout_rejected(self):
        self.cache["CMAKE_HOME_DIRECTORY"] = str(self.build)
        with self.assertRaisesRegex(ValueError, "different source checkout"):
            package.build_record()


if __name__ == "__main__":
    unittest.main()
