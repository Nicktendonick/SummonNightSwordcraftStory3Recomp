"""ROM-free checks of the private UI harness, not UI or gameplay acceptance."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import validate_portable_ui as ui


class UiObservationTests(unittest.TestCase):
    def test_current_language_expectations(self):
        self.assertEqual(ui.ENGLISH_SAVE, 'english-1.1.1f.eep')
        self.assertEqual(ui.ENGLISH_LABEL, 'English (translation 1.1.1.f)')

    def test_launcher_command_does_not_skip_preboot(self):
        command = ui.launcher_command(Path('synthetic package'))
        self.assertIn('--launcher', command)
        self.assertEqual(command[-1], str(Path('synthetic package/Runtime/game.toml')))
        for skip in ('--tcp', '--steps', '--frames', '--no-window', '--dump-bmp',
                     '--dump-png', '--load-state', '--rom', '--help', '-h', '--no-launcher'):
            self.assertNotIn(skip, command)

    def test_scaler_flags_and_effect_values(self):
        for flags, scaling in (((0, 0, 0), 0), ((1, 0, 0), 1), ((0, 1, 0), 2), ((0, 0, 1), 3)):
            log = '[launcher:filters] scaler=test linear=%d sharp=%d smooth=%d effect=2 strength=42' % flags
            self.assertEqual(ui.launcher_filters(log), [(scaling, 2, 42)])
        self.assertEqual(ui.launcher_filters('[launcher:ui] organized=1'), [])

    def test_runtime_observations_keep_order(self):
        log = '[runtime-filters] scaling=2 effect=0 strength=35\n[runtime-filters] scaling=3 effect=1 strength=42'
        self.assertEqual(ui.runtime_filters(log), [(2, 0, 35), (3, 1, 42)])

    def test_selected_is_not_yet_applied(self):
        log = '[runtime-aspect] selected=1 applied=2 host=384 guest=240'
        with self.assertRaises(AssertionError):
            ui.verify_applied_aspect(log, 1, 284)
        ui.verify_applied_aspect(log + '\n[runtime-aspect] selected=1 applied=1 host=284 guest=240', 1, 284)

    def test_applied_width_and_native_guest_are_required(self):
        for log in ('', '[runtime-aspect] selected=1 applied=1 host=384 guest=240',
                    '[runtime-aspect] selected=1 applied=1 host=284 guest=284',
                    '[runtime-aspect] selected=1 applied=1 host=284 guest=240\n'
                    '[runtime-aspect] selected=2 applied=2 host=384 guest=240'):
            with self.subTest(log=log), self.assertRaises(AssertionError):
                ui.verify_applied_aspect(log, 1, 284)


class ReportIntegrityTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / 'validation'
        base.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='ui-harness-test-', dir=base)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package = self.root / 'package'
        self.package.mkdir()
        self.payload = self.package / 'synthetic.txt'
        self.payload.write_text('synthetic package, not an executable')
        self.input = self.root / 'private.txt'
        self.input.write_text('synthetic input, not a ROM')
        self.protected = {str(self.input): ui.digest(self.input)}
        self.before = ui.tree(self.package)

    def finish(self, passed=True):
        report = dict(passed=passed)
        ui.finalize_report(report, self.protected, self.package, self.before)
        return report

    def test_success_requires_all_invariants(self):
        self.assertTrue(self.finish()['passed'])

    def test_prior_failure_stays_failed(self):
        self.assertFalse(self.finish(False)['passed'])

    def test_changed_or_missing_input_cannot_report_success(self):
        self.input.write_text('changed')
        self.assertFalse(self.finish()['passed'])
        self.input.unlink()
        self.assertFalse(self.finish()['passed'])

    def test_changed_payload_cannot_report_success(self):
        self.payload.write_text('changed')
        report = self.finish()
        self.assertFalse(report['package_unchanged'])
        self.assertFalse(report['passed'])

    def test_new_package_data_cannot_report_success(self):
        (self.package / 'accidental-save.txt').write_text('synthetic')
        self.assertFalse(self.finish()['passed'])


if __name__ == '__main__':
    unittest.main()
