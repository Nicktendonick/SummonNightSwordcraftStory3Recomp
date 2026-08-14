#!/usr/bin/env python3
"""Audit a deterministic Swordcraft 3 route in Native and 16:9 modes.

The report ranks evidence-backed candidates. It deliberately does not claim
that image heuristics can prove artistic intent or replace owner play-testing.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import html
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib


NATIVE_WIDTH = 240
HEIGHT = 160
DEFAULT_WIDE_WIDTH = 284
TRACE_PREFIX = "swordcraft3_widescreen_frame="
LAYER_MASKS = {
    "composite": 0x1F,
    "bg0": 0x01,
    "bg1": 0x02,
    "bg2": 0x04,
    "bg3": 0x08,
    "obj": 0x10,
}
# `battle_authored` is retained for re-analysis of preserved captures made
# before the route audit disproved the authored-BG1 assumption and renamed the
# live policy to `battle_reflect`.
AUTHORED_POLICIES = {
    "field_true_map", "field_reflect", "battle_reflect", "battle_authored"
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--bios", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--input-replay", type=Path)
    parser.add_argument("--load-state", type=Path)
    parser.add_argument("--start", type=int, default=1)
    parser.add_argument("--end", type=int, required=True)
    parser.add_argument("--step", type=int, default=12)
    parser.add_argument("--wide-width", type=int, default=DEFAULT_WIDE_WIDTH)
    parser.add_argument(
        "--layers", default="composite",
        help="wide captures: comma-separated composite,bg0,bg1,bg2,bg3,obj")
    parser.add_argument("--timeout", type=float, default=900.0)
    parser.add_argument("--max-findings", type=int, default=500)
    parser.add_argument("--reuse-capture", action="store_true")
    parser.add_argument(
        "--strict-static", action=argparse.BooleanOptionalAction, default=True,
        help="reject dispatch misses/interpreted instructions (default: on)")
    return parser.parse_args()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _paeth(a: int, b: int, c: int) -> int:
    estimate = a + b - c
    pa, pb, pc = abs(estimate - a), abs(estimate - b), abs(estimate - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def read_png_rgb(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError(f"not a PNG: {path}")
    pos = 8
    width = height = None
    payload = bytearray()
    while pos + 12 <= len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        kind = data[pos + 4:pos + 8]
        body_start = pos + 8
        body_end = body_start + length
        if body_end + 4 > len(data):
            raise RuntimeError(f"truncated PNG chunk: {path}")
        body = data[body_start:body_end]
        if kind == b"IHDR":
            if len(body) != 13:
                raise RuntimeError(f"invalid PNG header: {path}")
            width, height, depth, color, compression, filtering, interlace = \
                struct.unpack(">IIBBBBB", body)
            if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
                raise RuntimeError(f"unsupported PNG format: {path}")
        elif kind == b"IDAT":
            payload.extend(body)
        elif kind == b"IEND":
            break
        pos = body_end + 4
    if not width or not height or not payload:
        raise RuntimeError(f"incomplete PNG: {path}")
    decoded = zlib.decompress(bytes(payload))
    stride = width * 3
    if len(decoded) != height * (stride + 1):
        raise RuntimeError(f"unexpected PNG payload size: {path}")
    output = bytearray(width * height * 3)
    prior = bytearray(stride)
    source = 0
    for y in range(height):
        filter_type = decoded[source]
        source += 1
        row = bytearray(decoded[source:source + stride])
        source += stride
        for index in range(stride):
            left = row[index - 3] if index >= 3 else 0
            up = prior[index]
            upper_left = prior[index - 3] if index >= 3 else 0
            if filter_type == 1:
                row[index] = (row[index] + left) & 0xFF
            elif filter_type == 2:
                row[index] = (row[index] + up) & 0xFF
            elif filter_type == 3:
                row[index] = (row[index] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[index] = (row[index] + _paeth(left, up, upper_left)) & 0xFF
            elif filter_type != 0:
                raise RuntimeError(f"unsupported PNG filter {filter_type}: {path}")
        output[y * stride:(y + 1) * stride] = row
        prior = row
    return width, height, bytes(output)


def crop_rgb(pixels: bytes, width: int, x0: int, x1: int) -> bytes:
    output = bytearray()
    for y in range(HEIGHT):
        start = (y * width + x0) * 3
        output.extend(pixels[start:start + (x1 - x0) * 3])
    return bytes(output)


def changed_pixel_count(a: bytes, b: bytes, threshold: int = 0) -> int:
    if len(a) != len(b) or not a:
        return max(len(a), len(b)) // 3
    changed = 0
    for offset in range(0, len(a), 3):
        delta = (abs(a[offset] - b[offset]) +
                 abs(a[offset + 1] - b[offset + 1]) +
                 abs(a[offset + 2] - b[offset + 2]))
        if delta > threshold:
            changed += 1
    return changed


def changed_fraction(a: bytes, b: bytes, threshold: int = 0) -> float:
    if len(a) != len(b) or not a:
        return 1.0
    return changed_pixel_count(a, b, threshold) / (len(a) // 3)


def nonblack_pixels(pixels: bytes) -> int:
    return sum(1 for offset in range(0, len(pixels), 3)
               if pixels[offset:offset + 3] != b"\0\0\0")


def nonbackdrop_pixels(pixels: bytes) -> tuple[int, bytes]:
    colors = Counter(pixels[offset:offset + 3]
                     for offset in range(0, len(pixels), 3))
    backdrop = colors.most_common(1)[0][0]
    return sum(count for color, count in colors.items() if color != backdrop), backdrop


def authored_black_backdrop(pixels: bytes, width: int,
                             left: int, right: int) -> bool:
    if left <= 0 or right <= 0:
        return False
    left_pixels = crop_rgb(pixels, width, 0, left)
    right_pixels = crop_rgb(pixels, width, width - right, width)
    if nonblack_pixels(left_pixels) or nonblack_pixels(right_pixels):
        return False
    center = crop_rgb(pixels, width, left, left + NATIVE_WIDTH)
    center_nonblack_ratio = nonblack_pixels(center) / (NATIVE_WIDTH * HEIGHT)
    # Portrait/dialogue cutscenes reuse the field register family while their
    # actual backdrop is black. UI art may touch the old native boundaries;
    # requiring a black majority in the whole native image prevents that art
    # from turning an intentional black stage into a false margin defect.
    return center_nonblack_ratio < 0.75


def edge_score(pixels: bytes, width: int, edge_x: int) -> dict:
    def difference(x: int) -> float:
        total = 0
        for y in range(HEIGHT):
            left = (y * width + x - 1) * 3
            right = left + 3
            total += sum(abs(pixels[left + channel] - pixels[right + channel])
                         for channel in range(3))
        return total / (HEIGHT * 3)

    edge = difference(edge_x)
    nearby = [difference(x) for x in range(max(1, edge_x - 5),
                                            min(width, edge_x + 6))
              if x != edge_x]
    baseline = sum(nearby) / len(nearby) if nearby else 0.0
    return {"edge": round(edge, 3), "nearby": round(baseline, 3),
            "ratio": round(edge / max(baseline, 1.0), 3),
            "excess": round(edge - baseline, 3)}


def finding(kind: str, confidence: str, frame: int, evidence: str,
            **extra) -> dict:
    item = {"kind": kind, "confidence": confidence, "frame": frame,
            "evidence": evidence}
    item.update(extra)
    return item


def parse_trace(path: Path, expected: list[int]) -> tuple[list[dict], list[dict]]:
    by_frame = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(TRACE_PREFIX):
            state = json.loads(line[len(TRACE_PREFIX):])
            by_frame[int(state["frame"])] = state
    missing = [frame for frame in expected if frame not in by_frame]
    findings = [finding(
        "capture_integrity", "exact", frame,
        "wide composite capture has no aligned scene-policy telemetry")
        for frame in missing]
    return [by_frame[frame] for frame in expected if frame in by_frame], findings


def read_coverage(path: Path, label: str, frame: int,
                  require_static: bool) -> tuple[dict | None, list[dict]]:
    if not path.is_file():
        return None, [finding(
            "capture_integrity", "exact", frame,
            f"{label} has no runtime coverage report", run=label)]
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return None, [finding(
            "capture_integrity", "exact", frame,
            f"{label} coverage report is malformed: {error}", run=label)]
    summary = {
        "ok": bool(state.get("ok")),
        "coverage": state.get("coverage"),
        "distinct_misses": int(state.get("distinct_misses", -1)),
        "interpreted_insns": int(state.get("interpreted_insns", -1)),
    }
    if require_static and not (
            summary["ok"] and summary["coverage"] == "FULLY_STATIC" and
            summary["distinct_misses"] == 0 and
            summary["interpreted_insns"] == 0):
        return summary, [finding(
            "capture_integrity", "exact", frame,
            f"{label} is not a zero-miss FULLY_STATIC run",
            run=label, coverage=summary)]
    return summary, []


def capture_run(args: argparse.Namespace, output_dir: Path, mode: str,
                layer: str, frames: list[int]) -> None:
    layer_dir = output_dir / "raw" / mode / layer
    layer_dir.mkdir(parents=True, exist_ok=True)
    run_env = os.environ.copy()
    run_env.update({
        "GBARECOMP_FRAMEDUMP_DIR": str(layer_dir.resolve()),
        "GBARECOMP_FRAMEDUMP_START": str(args.start),
        "GBARECOMP_FRAMEDUMP_STEP": str(args.step),
        "GBARECOMP_FRAMEDUMP_COUNT": str(len(frames)),
        "GBARECOMP_UNTHROTTLED_CAPTURE": "1",
        "GBARECOMP_INPUT_REPLAY_EXCLUSIVE": "1",
        "GBARECOMP_DEBUG_LAYER_MASK": str(LAYER_MASKS[layer]),
        "GBARECOMP_NO_VSYNC": "1",
        # SDL joystick discovery is independent from the dummy video driver.
        # Exclude all real controllers even when the executable predates the
        # runtime's exclusive-replay guard.
        "SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT": "0x0000/0x0000",
        "SDL_JOYSTICK_HIDAPI": "0",
        "SDL_JOYSTICK_RAWINPUT": "0",
        "SDL_XINPUT_ENABLED": "0",
        "GBARECOMP_COVERAGE_JSON": str((layer_dir / "coverage.json").resolve()),
        "GBARECOMP_MISS_FRAG": str((layer_dir / "misses.toml.frag").resolve()),
    })
    if args.strict_static:
        run_env["GBARECOMP_STRICT_STATIC"] = "1"
        run_env.pop("GBARECOMP_SELFHEAL_RECOMPILE", None)
    else:
        run_env.pop("GBARECOMP_STRICT_STATIC", None)
    if args.input_replay:
        run_env["GBARECOMP_INPUT_REPLAY"] = str(args.input_replay.resolve())
    if mode == "wide" and layer == "composite":
        run_env.update({
            "SWORDCRAFT3_WS_AUDIT": "1",
            "SWORDCRAFT3_WS_AUDIT_START": str(args.start),
            "SWORDCRAFT3_WS_AUDIT_STEP": str(args.step),
        })
    command = [
        str(args.executable.resolve()), "--window", "--quiet", "--scale", "1",
        "--volume", "0", "--frames", str(args.end + args.step + 2),
        "--view-width", str(NATIVE_WIDTH if mode == "native" else args.wide_width),
        "--save", str((layer_dir / "audit-save.eep").resolve()),
        "--bios", str(args.bios.resolve()), "--rom", str(args.rom.resolve()),
    ]
    if args.load_state:
        command += ["--load-state", str(args.load_state.resolve())]
    command.append(str(args.config.resolve()))
    with (layer_dir / "stdout.log").open("w", encoding="utf-8") as stdout, \
            (layer_dir / "stderr.log").open("w", encoding="utf-8") as stderr:
        result = subprocess.run(
            command, cwd=str(args.config.resolve().parent), env=run_env,
            stdout=stdout, stderr=stderr, timeout=args.timeout, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"{mode}/{layer} capture exited with {result.returncode}; "
            f"see {layer_dir / 'stderr.log'}")


def load_images(output_dir: Path, mode: str, layers: tuple[str, ...],
                expected: list[int]) -> tuple[dict, list[dict]]:
    images = {}
    findings = []
    for layer in layers:
        layer_images = {}
        for frame in expected:
            path = output_dir / "raw" / mode / layer / f"f_{frame:06d}.png"
            if not path.is_file():
                findings.append(finding(
                    "capture_integrity", "exact", frame,
                    f"{mode}/{layer}/{path.name} is missing", mode=mode,
                    layer=layer))
                continue
            try:
                width, height, pixels = read_png_rgb(path)
            except RuntimeError as error:
                findings.append(finding(
                    "capture_integrity", "exact", frame,
                    f"{mode}/{layer}/{path.name} could not be decoded: {error}",
                    mode=mode, layer=layer))
                continue
            expected_width = NATIVE_WIDTH if mode == "native" else None
            if height != HEIGHT or (expected_width and width != expected_width):
                findings.append(finding(
                    "capture_integrity", "exact", frame,
                    f"unexpected {mode}/{layer} dimensions {width}x{height}",
                    mode=mode, layer=layer))
                continue
            layer_images[frame] = (width, height, pixels, path)
        images[layer] = layer_images
    return images, findings


def analyze_center(native: dict, wide: dict, telemetry: dict) -> list[dict]:
    output = []
    for frame in sorted(set(native) & set(wide)):
        native_width, _, native_pixels, _ = native[frame]
        wide_width, _, wide_pixels, _ = wide[frame]
        extra_left = int(telemetry.get(frame, {}).get(
            "extra_left", (wide_width - NATIVE_WIDTH) // 2))
        center = crop_rgb(wide_pixels, wide_width, extra_left,
                          extra_left + NATIVE_WIDTH)
        mismatches = changed_pixel_count(native_pixels, center)
        if native_width != NATIVE_WIDTH or mismatches:
            output.append(finding(
                "native_center_mismatch", "exact", frame,
                f"{mismatches} of {NATIVE_WIDTH * HEIGHT} native pixels differ "
                "between aligned Native and Wide runs",
                mismatch_pixels=mismatches))
    return output


def analyze_policy_and_margins(wide: dict, telemetry: dict) -> tuple[list[dict], list[dict]]:
    candidates = []
    safe = []
    seen_safe = set()
    for frame, (width, _, pixels, _) in sorted(wide.items()):
        state = telemetry.get(frame)
        if not state:
            continue
        left = int(state["extra_left"])
        right = int(state["extra_right"])
        left_pixels = crop_rgb(pixels, width, 0, left)
        right_pixels = crop_rgb(pixels, width, width - right, width)
        policy = state["policy"]
        palette_backdrop_only = (int(state["dispcnt"]) & 0x0F00) == 0
        if policy in {"pillarbox", "unsupported_mode"} and not palette_backdrop_only:
            leaks = nonblack_pixels(left_pixels) + nonblack_pixels(right_pixels)
            if leaks:
                candidates.append(finding(
                    "pillarbox_policy_leak", "exact", frame,
                    f"fail-closed policy produced {leaks} non-black margin pixels",
                    policy=policy, nonblack_margin_pixels=leaks))
            key = (policy, int(state["dispcnt"]), tuple(state["bgcnt"]))
            if key not in seen_safe:
                seen_safe.add(key)
                safe.append(finding(
                    "unclassified_scene_pillarboxed", "safe", frame,
                    "scene remained deliberately centered because no authored "
                    "margin policy matched", policy=policy))
        elif policy in AUTHORED_POLICIES:
            if authored_black_backdrop(pixels, width, left, right):
                key = ("black_backdrop", policy, tuple(state["bgcnt"]),
                       tuple(state["hofs"]), tuple(state["vofs"]))
                if key not in seen_safe:
                    seen_safe.add(key)
                    safe.append(finding(
                        "authored_black_backdrop", "safe", frame,
                        "both margins and most of the native backdrop are "
                        "black; portrait/dialogue art remains centered",
                        policy=policy))
                continue
            adjacent = {
                "left": crop_rgb(pixels, width, left,
                                 left + max(left, 1)),
                "right": crop_rgb(pixels, width,
                                  left + NATIVE_WIDTH - max(right, 1),
                                  left + NATIVE_WIDTH),
            }
            for side, margin in (("left", left_pixels), ("right", right_pixels)):
                edge_band = adjacent[side]
                edge_nonblack_ratio = (nonblack_pixels(edge_band) /
                                       max(len(edge_band) // 3, 1))
                if margin and nonblack_pixels(margin) == 0 and \
                        edge_nonblack_ratio > 0.10:
                    candidates.append(finding(
                        "authored_margin_blank", "strong", frame,
                        f"{side} margin is entirely black while policy "
                        f"{policy} claims authored continuation",
                        policy=policy, side=side,
                        adjacent_edge_nonblack=round(edge_nonblack_ratio, 5)))
    return candidates, safe


def analyze_seams(wide: dict, telemetry: dict) -> list[dict]:
    candidates = []
    for frame, (width, _, pixels, _) in sorted(wide.items()):
        state = telemetry.get(frame)
        if not state or state["policy"] not in AUTHORED_POLICIES:
            continue
        left = int(state["extra_left"])
        right = int(state["extra_right"])
        if authored_black_backdrop(pixels, width, left, right):
            continue
        for side, edge in (("left", left), ("right", left + NATIVE_WIDTH)):
            score = edge_score(pixels, width, edge)
            if score["ratio"] >= 3.0 and score["excess"] >= 18.0:
                candidates.append(finding(
                    "native_boundary_seam", "strong", frame,
                    f"{side} old native boundary is {score['ratio']:.2f}x "
                    "stronger than nearby column edges",
                    side=side, score=score, policy=state["policy"]))
    by_key = {(item["frame"], item["side"]): item for item in candidates}
    frames = sorted(wide)
    retained = []
    for side in ("left", "right"):
        run = []
        for frame in frames + [None]:
            item = by_key.get((frame, side)) if frame is not None else None
            if item:
                run.append(item)
                continue
            if len(run) >= 2:
                representative = max(
                    run, key=lambda value: value["score"]["excess"])
                representative["run_start"] = run[0]["frame"]
                representative["run_end"] = run[-1]["frame"]
                representative["sample_count"] = len(run)
                representative["evidence"] += (
                    f"; persisted across {len(run)} adjacent samples "
                    f"({run[0]['frame']}..{run[-1]['frame']})")
                retained.append(representative)
            run = []
    return retained


def analyze_temporal_freeze(wide: dict, telemetry: dict) -> list[dict]:
    frames = sorted(set(wide) & set(telemetry))
    raw = []
    for previous_frame, frame in zip(frames, frames[1:]):
        prior = telemetry[previous_frame]
        state = telemetry[frame]
        if prior["policy"] != state["policy"] or \
                state["policy"] not in AUTHORED_POLICIES:
            continue
        margin_mask = int(state["margin_layers"])
        scroll_changed = any(
            margin_mask & (1 << layer) and
            (prior["hofs"][layer] != state["hofs"][layer] or
             prior["vofs"][layer] != state["vofs"][layer])
            for layer in range(4))
        if not scroll_changed:
            continue
        width, _, pixels, _ = wide[frame]
        prior_pixels = wide[previous_frame][2]
        left = int(state["extra_left"])
        right = int(state["extra_right"])
        center_delta = changed_fraction(
            crop_rgb(prior_pixels, width, left, left + NATIVE_WIDTH),
            crop_rgb(pixels, width, left, left + NATIVE_WIDTH), 24)
        margin_delta = changed_fraction(
            crop_rgb(prior_pixels, width, 0, left) +
            crop_rgb(prior_pixels, width, width - right, width),
            crop_rgb(pixels, width, 0, left) +
            crop_rgb(pixels, width, width - right, width), 24)
        if center_delta >= 0.05 and margin_delta <= 0.002:
            raw.append(finding(
                "margin_temporal_freeze", "suspect", frame,
                "authorized BG scroll and native content changed, but both "
                "margins remained nearly static",
                reference_frame=previous_frame, policy=state["policy"],
                center_changed=round(center_delta, 5),
                margin_changed=round(margin_delta, 5)))
    raw_frames = {item["frame"] for item in raw}
    return [item for item in raw if item["reference_frame"] in raw_frames or
            any(other.get("reference_frame") == item["frame"] for other in raw)]


def analyze_obj_margin(images: dict, telemetry: dict) -> list[dict]:
    output = []
    for frame, (width, _, pixels, _) in sorted(images.items()):
        state = telemetry.get(frame)
        if not state or not state.get("obj_native_clip"):
            continue
        left = int(state["extra_left"])
        right = int(state["extra_right"])
        margin = (crop_rgb(pixels, width, 0, left) +
                  crop_rgb(pixels, width, width - right, width))
        # An isolated OBJ surface is overwhelmingly the palette backdrop.
        # Derive it from the whole frame, not only the narrow margins, so a
        # large leaking sprite cannot vote itself into becoming "background".
        _, backdrop = nonbackdrop_pixels(pixels)
        visible = sum(1 for offset in range(0, len(margin), 3)
                      if margin[offset:offset + 3] != backdrop)
        if visible:
            output.append(finding(
                "obj_native_clip_leak", "exact", frame,
                f"OBJ-only capture has {visible} non-backdrop margin pixels "
                "while native clipping is enabled",
                visible_margin_pixels=visible, backdrop=backdrop.hex(), layer="obj"))
    return output


def deduplicate(items: list[dict], maximum: int) -> list[dict]:
    rank = {"exact": 0, "strong": 1, "suspect": 2, "safe": 3}
    output = []
    seen = set()
    for item in sorted(items, key=lambda value: (
            rank.get(value["confidence"], 9), value["frame"], value["kind"])):
        key = (item["kind"], item.get("side"), item.get("layer"),
               item["frame"] // 60 if item["kind"] == "authored_margin_blank"
               else item["frame"])
        if key in seen:
            continue
        seen.add(key)
        output.append(item)
        if len(output) >= maximum:
            break
    return output


def attach_evidence(output_dir: Path, findings: list[dict], native: dict,
                    wide_layers: dict) -> None:
    for item in findings:
        paths = []
        seen_paths = set()
        layer = item.get("layer", "composite")
        for mode, images in (("native", native),
                             (f"wide/{layer}", wide_layers.get(layer, {})),
                             ("wide/composite", wide_layers.get("composite", {}))):
            for frame in dict.fromkeys((item["frame"], item.get("reference_frame"))):
                if frame is None or frame not in images:
                    continue
                path = images[frame][3]
                relative = path.relative_to(output_dir).as_posix()
                if relative in seen_paths:
                    continue
                seen_paths.add(relative)
                paths.append({"label": mode, "path": relative})
        item["images"] = paths


def write_html(output_dir: Path, report: dict) -> None:
    rows = []
    for item in report["findings"]:
        pictures = "".join(
            f"<figure><img src='{html.escape(image['path'])}'><figcaption>"
            f"{html.escape(image['label'])}</figcaption></figure>"
            for image in item.get("images", []))
        rows.append(
            f"<tr><td>{html.escape(item['confidence'])}</td>"
            f"<td>{html.escape(item['kind'])}</td><td>{item['frame']}</td>"
            f"<td>{html.escape(item['evidence'])}</td>"
            f"<td class='shots'>{pictures}</td></tr>")
    if not rows:
        rows.append("<tr><td colspan='5'>No encoded detector fired. This is "
                    "not a correctness proof.</td></tr>")
    safe_rows = "".join(
        f"<tr><td>{html.escape(item['kind'])}</td><td>{item['frame']}</td>"
        f"<td>{html.escape(item['evidence'])}</td></tr>"
        for item in report["safe_observations"])
    summary = html.escape(json.dumps(report["summary"], indent=2))
    document = f"""<!doctype html>
<meta charset="utf-8"><title>Swordcraft 3 widescreen route audit</title>
<style>
body{{background:#111;color:#eee;font:15px system-ui;margin:2rem}}
table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #555;padding:.5rem;vertical-align:top}}
th{{background:#282828;position:sticky;top:0}}img{{width:284px;image-rendering:pixelated}}
figure{{display:inline-block;margin:.25rem}}figcaption{{color:#aaa}}pre{{background:#1d1d1d;padding:1rem}}
</style>
<h1>Summon Night: Swordcraft Story 3 widescreen route audit</h1>
<p>This deterministic report ranks candidates. Runtime policy provenance and a
pixel-identical native center are strong evidence, but neither proves that
margin art, object intent, priority, or animation is artistically correct.</p>
<pre>{summary}</pre>
<table><thead><tr><th>Confidence</th><th>Detector</th><th>Frame</th><th>Evidence</th><th>Images</th></tr></thead>
<tbody>{''.join(rows)}</tbody></table>
<h2>Safe observations</h2>
<table><thead><tr><th>Kind</th><th>Frame</th><th>Evidence</th></tr></thead>
<tbody>{safe_rows or "<tr><td colspan='3'>None</td></tr>"}</tbody></table>
"""
    (output_dir / "index.html").write_text(document, encoding="utf-8")


def main() -> int:
    args = parse_args()
    for path in (args.executable, args.rom, args.bios, args.config,
                 args.input_replay, args.load_state):
        if path and not path.expanduser().is_file():
            raise RuntimeError(f"required input does not exist: {path}")
    if args.start < 1 or args.end < args.start or args.step < 1 or \
            args.max_findings < 1 or args.wide_width <= NATIVE_WIDTH:
        raise RuntimeError("invalid frame range, step, finding limit, or wide width")
    layers = tuple(item.strip().lower() for item in args.layers.split(",")
                   if item.strip())
    if "composite" not in layers or len(layers) != len(set(layers)):
        raise RuntimeError("--layers must contain unique names including composite")
    unknown = [layer for layer in layers if layer not in LAYER_MASKS]
    if unknown:
        raise RuntimeError(f"unknown layer(s): {', '.join(unknown)}")
    output_dir = args.output_dir.expanduser().resolve()
    if args.reuse_capture:
        if not (output_dir / "raw" / "wide" / "composite" / "stderr.log").is_file():
            raise RuntimeError("--reuse-capture requires an existing raw capture")
    elif output_dir.exists() and any(output_dir.iterdir()):
        raise RuntimeError(f"output directory is not empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    expected = list(range(args.start, args.end + 1, args.step))

    if not args.reuse_capture:
        print(f"capturing Native composite ({len(expected)} samples)...", flush=True)
        capture_run(args, output_dir, "native", "composite", expected)
        for layer in layers:
            print(f"capturing Wide {layer} ({len(expected)} samples)...", flush=True)
            capture_run(args, output_dir, "wide", layer, expected)

    native_layers, integrity = load_images(
        output_dir, "native", ("composite",), expected)
    wide_layers, more_integrity = load_images(
        output_dir, "wide", layers, expected)
    integrity.extend(more_integrity)
    coverage = {}
    for mode, layer in (("native", "composite"), *
                        (("wide", layer) for layer in layers)):
        label = f"{mode}/{layer}"
        state, coverage_findings = read_coverage(
            output_dir / "raw" / mode / layer / "coverage.json",
            label, args.start, args.strict_static)
        coverage[label] = state
        integrity.extend(coverage_findings)
    trace_path = output_dir / "raw" / "wide" / "composite" / "stderr.log"
    telemetry, trace_findings = parse_trace(trace_path, expected)
    integrity.extend(trace_findings)
    telemetry_by_frame = {int(item["frame"]): item for item in telemetry}
    native = native_layers["composite"]
    wide = wide_layers["composite"]

    findings = list(integrity)
    findings.extend(analyze_center(native, wide, telemetry_by_frame))
    policy_findings, safe = analyze_policy_and_margins(wide, telemetry_by_frame)
    findings.extend(policy_findings)
    findings.extend(analyze_seams(wide, telemetry_by_frame))
    findings.extend(analyze_temporal_freeze(wide, telemetry_by_frame))
    if "obj" in wide_layers:
        findings.extend(analyze_obj_margin(wide_layers["obj"], telemetry_by_frame))
    findings = deduplicate(findings, args.max_findings)
    safe = deduplicate(safe, args.max_findings)
    attach_evidence(output_dir, findings, native, wide_layers)

    counts = Counter(item["kind"] for item in findings)
    policy_counts = Counter(item["policy"] for item in telemetry)
    coverage_states = [state for state in coverage.values() if state]
    coverage_fully_static = (
        len(coverage_states) == len(coverage) and all(
            state["ok"] and state["coverage"] == "FULLY_STATIC" and
            state["distinct_misses"] == 0 and
            state["interpreted_insns"] == 0
            for state in coverage_states))
    coverage_signatures = {
        (state["ok"], state["coverage"], state["distinct_misses"],
         state["interpreted_insns"])
        for state in coverage_states
    }
    coverage_matches_across_runs = (
        len(coverage_states) == len(coverage) and
        len(coverage_signatures) == 1)
    report = {
        "schema": "swordcraft3-widescreen-route-audit-v1",
        "range": {"start": args.start, "end": args.end, "step": args.step,
                  "samples": len(expected)},
        "layers": list(layers),
        "wide_width": args.wide_width,
        "coverage": coverage,
        "inputs": {
            "executable": {"basename": args.executable.name,
                           "sha256": sha256_file(args.executable)},
            "rom": {"basename": args.rom.name, "sha256": sha256_file(args.rom)},
            "bios": {"basename": args.bios.name, "sha256": sha256_file(args.bios)},
            "input_replay": ({"basename": args.input_replay.name,
                              "sha256": sha256_file(args.input_replay)}
                             if args.input_replay else None),
            "load_state": ({"basename": args.load_state.name,
                            "sha256": sha256_file(args.load_state)}
                           if args.load_state else None),
        },
        "summary": {
            "findings_retained": len(findings),
            "safe_observations_retained": len(safe),
            "capture_integrity_errors": counts.get("capture_integrity", 0),
            "accepted_for_visual_review":
                counts.get("capture_integrity", 0) == 0,
            "release_validation_eligible":
                counts.get("capture_integrity", 0) == 0 and
                coverage_fully_static,
            "diagnostic_comparison_eligible":
                counts.get("capture_integrity", 0) == 0 and
                coverage_matches_across_runs,
            "coverage_fully_static": coverage_fully_static,
            "coverage_matches_across_runs": coverage_matches_across_runs,
            "by_detector": dict(sorted(counts.items())),
            "by_policy": dict(sorted(policy_counts.items())),
            "limitations": [
                "A clean report means no encoded detector fired, not no defect exists.",
                "Composite images cannot prove world/map identity without a map-data sidecar.",
                "Seams and temporal freezes remain heuristics and require visual review.",
                "Object intent and activation/culling need semantic state or a reference trace.",
                "Matching non-static coverage permits diagnosis, not release acceptance.",
                "Run a coarse route first, then recapture suspect intervals at step 1.",
            ],
        },
        "findings": findings,
        "safe_observations": safe,
    }
    (output_dir / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_html(output_dir, report)
    print(json.dumps({
        "report": str(output_dir / "report.json"),
        "html": str(output_dir / "index.html"),
        "findings": len(findings), "safe_observations": len(safe),
        "by_detector": dict(sorted(counts.items())),
    }))
    return 2 if counts.get("capture_integrity", 0) else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError,
            json.JSONDecodeError, ValueError, zlib.error) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
