#!/usr/bin/env python3
"""Fail closed when a route-audit report violates its capability contract."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    report = json.loads(args.report.read_text(encoding="utf-8"))
    errors: list[str] = []

    expected_range = contract["frame_range"]
    for key in ("start", "end", "step"):
        if report.get("range", {}).get(key) != expected_range[key]:
            errors.append(f"range.{key} does not match contract")

    policies = report.get("summary", {}).get("by_policy", {})
    for policy in contract.get("required_policies", []):
        if policies.get(policy, 0) <= 0:
            errors.append(f"required policy was not observed: {policy}")

    if contract.get("require_native_wide_guest_state_match") and not report.get(
            "summary", {}).get("guest_state_matches_across_native_wide", False):
        errors.append("Native and Wide guest-state traces do not match")

    allowlist = contract.get("allowed_findings", [])
    unallowed = []
    for finding in report.get("findings", []):
        if any(all(finding.get(key) == allowed.get(key)
                   for key in ("kind", "frame", "side")
                   if key in allowed)
               for allowed in allowlist):
            continue
        unallowed.append(finding)
    actual: dict[str, int] = {}
    for finding in unallowed:
        kind = finding.get("kind", "unknown")
        actual[kind] = actual.get(kind, 0) + 1
    for detector, maximum in contract.get(
            "maximum_findings_by_detector", {}).items():
        count = actual.get(detector, 0)
        if count > maximum:
            errors.append(f"{detector}: {count} exceeds maximum {maximum}")

    if report.get("summary", {}).get("capture_integrity_errors", 1) != 0:
        errors.append("capture integrity errors were reported")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print("PASS: widescreen route satisfies the capability contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
