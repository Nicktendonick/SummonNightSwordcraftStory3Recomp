import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "audit_widescreen_route", ROOT / "tools" / "audit_widescreen_route.py")
AUDIT = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(AUDIT)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    rows = b"".join(
        b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
        for y in range(height))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
                                        8, 2, 0, 0, 0)) +
        png_chunk(b"IDAT", zlib.compress(rows)) + png_chunk(b"IEND", b""))


def image(width: int, color: bytes = b"\0\0\0") -> bytearray:
    return bytearray(color * (width * AUDIT.HEIGHT))


def set_pixel(pixels: bytearray, width: int, x: int, y: int,
              color: bytes) -> None:
    offset = (y * width + x) * 3
    pixels[offset:offset + 3] = color


def state(frame: int, policy: str = "field_reflect") -> dict:
    return {
        "frame": frame, "view_width": 284, "extra_left": 22,
        "extra_right": 22, "policy": policy, "dispcnt": 0x1F00,
        "bg_mode": 0, "forced_blank": False, "visible_layers": 0xF,
        "margin_layers": 0xE, "mirrored_layers": 0xE,
        "wrapped_layers": 0, "pillarbox": policy not in AUDIT.AUTHORED_POLICIES,
        "obj_native_clip": True, "bgcnt": [0, 0, 0, 0],
        "hofs": [0, 0, 0, 0], "vofs": [0, 0, 0, 0],
        "winin": 0, "winout": 0,
    }


class WidescreenRouteAuditTests(unittest.TestCase):
    def test_reads_generated_rgb_png(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.png"
            pixels = bytes([0x20, 0x0A, 0x09, 255, 1, 2])
            write_png(path, 2, 1, pixels)
            self.assertEqual(AUDIT.read_png_rgb(path), (2, 1, pixels))

    def test_native_center_comparison_is_exact(self):
        native_pixels = bytes(image(240, b"\x10\x20\x30"))
        wide_pixels = image(284, b"\x00\x00\x00")
        for y in range(AUDIT.HEIGHT):
            start = (y * 284 + 22) * 3
            wide_pixels[start:start + 240 * 3] = \
                native_pixels[y * 240 * 3:(y + 1) * 240 * 3]
        native = {12: (240, 160, native_pixels, Path("native.png"))}
        wide = {12: (284, 160, bytes(wide_pixels), Path("wide.png"))}
        telemetry = {12: state(12)}
        self.assertEqual(AUDIT.analyze_center(native, wide, telemetry), [])
        set_pixel(wide_pixels, 284, 22, 0, b"\xFF\x00\x00")
        findings = AUDIT.analyze_center(
            native, {12: (284, 160, bytes(wide_pixels), Path("wide.png"))},
            telemetry)
        self.assertEqual(findings[0]["kind"], "native_center_mismatch")
        self.assertEqual(findings[0]["mismatch_pixels"], 1)

    def test_pillarbox_leak_and_safe_observation_are_separate(self):
        pixels = image(284)
        set_pixel(pixels, 284, 0, 0, b"\x01\x02\x03")
        telemetry = {24: state(24, "pillarbox")}
        findings, safe = AUDIT.analyze_policy_and_margins(
            {24: (284, 160, bytes(pixels), Path("wide.png"))}, telemetry)
        self.assertEqual(findings[0]["kind"], "pillarbox_policy_leak")
        self.assertEqual(safe[0]["kind"], "unclassified_scene_pillarboxed")

    def test_black_portrait_stage_is_a_safe_observation(self):
        pixels = image(284)
        for y in range(40, 120):
            for x in range(80, 160):
                set_pixel(pixels, 284, x, y, b"\x80\x40\x20")
        findings, safe = AUDIT.analyze_policy_and_margins(
            {24: (284, 160, bytes(pixels), Path("wide.png"))},
            {24: state(24, "field_reflect")})
        self.assertEqual(findings, [])
        self.assertEqual(safe[0]["kind"], "authored_black_backdrop")

    def test_battle_hybrid_receives_authored_margin_checks(self):
        self.assertIn("battle_hybrid", AUDIT.AUTHORED_POLICIES)
        pixels = image(284)
        for y in range(160):
            for x in range(22, 262):
                set_pixel(pixels, 284, x, y, b"\xFF\xFF\xFF")
        telemetry = {24: state(24, "battle_hybrid")}
        findings, safe = AUDIT.analyze_policy_and_margins(
            {24: (284, 160, bytes(pixels), Path("wide.png"))}, telemetry)
        self.assertEqual(findings[0]["kind"], "authored_margin_blank")
        self.assertEqual(safe, [])

    def test_battle_loop_receives_authored_margin_checks(self):
        self.assertIn("battle_loop", AUDIT.AUTHORED_POLICIES)
        pixels = image(284)
        for y in range(160):
            for x in range(22, 262):
                set_pixel(pixels, 284, x, y, b"\xFF\xFF\xFF")
        telemetry = {24: state(24, "battle_loop")}
        findings, safe = AUDIT.analyze_policy_and_margins(
            {24: (284, 160, bytes(pixels), Path("wide.png"))}, telemetry)
        self.assertEqual(findings[0]["kind"], "authored_margin_blank")
        self.assertEqual(safe, [])

    def test_battle_natural_receives_authored_margin_checks(self):
        self.assertIn("battle_natural", AUDIT.AUTHORED_POLICIES)
        pixels = image(284)
        for y in range(160):
            for x in range(22, 262):
                set_pixel(pixels, 284, x, y, b"\xFF\xFF\xFF")
        telemetry = {24: state(24, "battle_natural")}
        findings, safe = AUDIT.analyze_policy_and_margins(
            {24: (284, 160, bytes(pixels), Path("wide.png"))}, telemetry)
        self.assertEqual(findings[0]["kind"], "authored_margin_blank")
        self.assertEqual(safe, [])

    def test_seam_requires_neighboring_samples(self):
        frames = {}
        telemetry = {}
        for frame in (12, 24):
            pixels = image(284)
            for y in range(160):
                for x in range(22, 284):
                    set_pixel(pixels, 284, x, y, b"\xFF\xFF\xFF")
            frames[frame] = (284, 160, bytes(pixels), Path("wide.png"))
            telemetry[frame] = state(frame)
        findings = AUDIT.analyze_seams(frames, telemetry)
        self.assertTrue(any(item["kind"] == "native_boundary_seam" and
                            item["side"] == "left" for item in findings))
        self.assertEqual(AUDIT.analyze_seams({12: frames[12]}, {12: state(12)}), [])

    def test_obj_clip_finding_uses_isolated_layer(self):
        pixels = image(284)
        set_pixel(pixels, 284, 3, 40, b"\xF8\x00\x00")
        findings = AUDIT.analyze_obj_margin(
            {12: (284, 160, bytes(pixels), Path("obj.png"))},
            {12: state(12)})
        self.assertEqual(findings[0]["kind"], "obj_native_clip_leak")

    def test_trace_parser_reports_missing_aligned_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "stderr.log"
            path.write_text(
                "noise\n" + AUDIT.TRACE_PREFIX + json.dumps(state(12)) + "\n",
                encoding="utf-8")
            telemetry, findings = AUDIT.parse_trace(path, [12, 24])
            self.assertEqual([item["frame"] for item in telemetry], [12])
            self.assertEqual(findings[0]["frame"], 24)
            self.assertEqual(findings[0]["kind"], "capture_integrity")

    def test_coverage_contract_rejects_non_static_reuse(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "coverage.json"
            path.write_text(json.dumps({
                "ok": True, "coverage": "PARTIAL", "distinct_misses": 1,
                "interpreted_insns": 12,
            }), encoding="utf-8")
            summary, findings = AUDIT.read_coverage(
                path, "wide/composite", 1200, True)
            self.assertEqual(summary["coverage"], "PARTIAL")
            self.assertEqual(findings[0]["kind"], "capture_integrity")
            self.assertEqual(findings[0]["run"], "wide/composite")

    def test_guest_state_comparison_reports_first_divergence(self):
        base = {key: "00" for key in AUDIT.GUEST_STATE_KEYS}
        native = {12: {"frame": 12, **base}, 24: {"frame": 24, **base}}
        changed = dict(base)
        changed["iwram"] = "01"
        wide = {12: {"frame": 12, **base}, 24: {"frame": 24, **changed}}
        findings = AUDIT.analyze_guest_state(native, wide)
        self.assertEqual(findings[0]["kind"],
                         "native_wide_guest_state_divergence")
        self.assertEqual(findings[0]["frame"], 24)
        self.assertEqual(findings[0]["fields"], ["iwram"])

    def test_state_trace_loader_fails_closed_on_missing_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "state.jsonl"
            path.write_text(json.dumps({
                "schema": "gbarecomp-state-trace-v1", "frame": 12,
                **{key: "00" for key in AUDIT.GUEST_STATE_KEYS},
            }) + "\n", encoding="utf-8")
            rows, findings = AUDIT.load_state_trace(
                path, [12, 24], "native/composite")
            self.assertEqual(sorted(rows), [12])
            self.assertEqual(findings[0]["kind"], "capture_integrity")
            self.assertEqual(findings[0]["frame"], 24)


if __name__ == "__main__":
    unittest.main()
