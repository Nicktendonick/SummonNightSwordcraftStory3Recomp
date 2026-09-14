"""Reject accidental mixing of incompatible cached and newly generated code."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('shadow_codegen',
    Path(__file__).resolve().parents[1] / 'tools/prepare_lab_shadow_codegen.py')
codegen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codegen)


class CorpusGuardTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name) / 'new'
        self.old = Path(self.temp.name) / 'old'
        self.out.mkdir()
        self.old.mkdir()
        for name, (pc, imm) in codegen.HOOKS.items():
            (self.old / name).write_text(f'value = 0x{imm}u;\n')
            (self.out / name).write_text(
                f'    uint32_t _imm_{pc} = runtime_thumb_alu_immediate(0x{pc}u, 0x{imm}u);\n'
                f'value = _imm_{pc};\n')
        for folder in (self.old, self.out):
            (folder / 'dispatch_table.cpp').write_text('same table\n')

    def test_only_reviewed_immediates_allowed(self):
        with contextlib.redirect_stdout(io.StringIO()):
            codegen.verify_corpus(self.out, self.old)

    def test_unrelated_drift_rejected(self):
        (self.out / 'dispatch_table.cpp').write_text('changed table\n')
        with self.assertRaises(ValueError):
            codegen.verify_corpus(self.out, self.old)

    def test_missing_hook_rejected(self):
        (self.out / 'recompiled_007.cpp').write_text((self.old / 'recompiled_007.cpp').read_text())
        with self.assertRaises(ValueError):
            codegen.verify_corpus(self.out, self.old)

    def test_changed_source_set_rejected(self):
        (self.out / 'extra.cpp').write_text('extra\n')
        with self.assertRaises(ValueError):
            codegen.verify_corpus(self.out, self.old)


if __name__ == '__main__':
    unittest.main()
