"""Read-only launch-plan tests; no game, ROM, BIOS, or GUI required.

Run: python -B -m unittest discover -s tests -p test_debug_capture_launcher.py
"""
import json
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which("powershell.exe")


@unittest.skipUnless(POWERSHELL, "Windows PowerShell is required")
class DebugCaptureLauncherTests(unittest.TestCase):
    def plan(self, *args):
        script = str(ROOT / "tools/launch_visible_debugger.ps1").replace("'", "''")
        # Only mock required-input existence. PlanOnly must never execute the
        # game or create session files, even on a source-only checkout.
        command = (
            "function Test-Path { param($LiteralPath, $PathType) return $true }; "
            f"& '{script}' -PlanOnly " + " ".join(args)
        )
        result = subprocess.run(
            [POWERSHELL, "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", command],
            capture_output=True, text=True, timeout=30,
        )
        return result

    def test_interactive_settings_are_not_overridden(self):
        result = self.plan("-ShowLauncher")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertTrue(plan["show_launcher"])
        self.assertIn("--launcher", plan["arguments"])
        for flag in ("--rom", "--view-width", "--resize-view", "--no-launcher"):
            self.assertNotIn(flag, plan["arguments"])
        self.assertEqual(set(plan["clear_environment"]), {
            "GBARECOMP_VIEW_WIDTH", "GBARECOMP_WIDESCREEN", "GBARECOMP_RESIZE_VIEW",
        })
        self.assertFalse(Path(plan["session"]).exists())
        self.assertTrue(Path(plan["session"]).is_relative_to(ROOT / "validation/visible-debugger"))
        env = plan["environment"]
        self.assertEqual(env["GBARECOMP_VISIBLE_DEBUGGER"], "1")
        self.assertEqual(env["SWORDCRAFT3_WS_DEBUG"], "1")
        for key in ("GBARECOMP_DEBUG_CAPTURE_DIR", "GBARECOMP_INPUT_RECORD",
                    "GBARECOMP_AUDIO_DUMP", "GBARECOMP_FRAME_PHASE",
                    "GBARECOMP_COVERAGE_JSON", "GBARECOMP_MISS_FRAG"):
            self.assertTrue(Path(env[key]).is_relative_to(Path(plan["session"])))
        args = plan["arguments"]
        self.assertEqual(Path(args[args.index("--save") + 1]).parent, Path(plan["session"]))

    def test_direct_launch_preserved(self):
        result = self.plan("-ViewWidth", "384")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        args = plan["arguments"]
        self.assertFalse(plan["show_launcher"])
        self.assertIn("--no-launcher", args)
        self.assertIn("--rom", args)
        self.assertEqual(args[args.index("--view-width") + 1], "384")
        self.assertEqual(plan["clear_environment"], [])

    def test_stock_uses_current_build(self):
        result = self.plan("-Edition", "Stock", "-ShowLauncher")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(Path(plan["executable"]), ROOT / "build-beta/SummonNightSwordcraftStory3Recomp.exe")

    def test_out_of_range_width_rejected(self):
        self.assertNotEqual(self.plan("-ViewWidth", "385").returncode, 0)

    def test_batch_opens_settings_without_forcing_width(self):
        batch = (ROOT / "Launch Debug Capture.bat").read_text()
        self.assertIn("-ShowLauncher", batch)
        self.assertNotIn("-ViewWidth", batch)


if __name__ == "__main__":
    unittest.main()
