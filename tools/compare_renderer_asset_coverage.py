#!/usr/bin/env python3
"""Fail when current renderer coverage introduces unresolved state keys."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


RESOLVED = {"renderable", "intentional_procedural"}


def state_key(row: dict) -> str:
    dimensions = json.dumps(
        row["state_dimensions"],
        sort_keys=True,
        separators=(",", ":"),
    )
    return f'{row["object_kind"]}|{dimensions}'


def unresolved(report: dict) -> dict[str, tuple[str, str]]:
    result: dict[str, tuple[str, str]] = {}
    for row in report["rows"]:
        if row["status"] in RESOLVED:
            continue
        result[state_key(row)] = (
            row["status"],
            row.get("failure_reason", ""),
        )
    return result


def compare(baseline: dict, current: dict) -> list[str]:
    if baseline.get("schema") != "aoe-renderer-asset-coverage-v2":
        raise ValueError("baseline has unsupported schema")
    if current.get("schema") != baseline.get("schema"):
        raise ValueError("current report schema differs from baseline")
    before = unresolved(baseline)
    after = unresolved(current)
    regressions: list[str] = []
    for key, finding in sorted(after.items()):
        if key not in before:
            regressions.append(
                f"new unresolved state: {key}: {finding[0]}: {finding[1]}"
            )
        elif before[key] != finding:
            regressions.append(
                f"changed unresolved state: {key}: "
                f"{before[key][0]} -> {finding[0]}: {finding[1]}"
            )
    return regressions


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--current", required=True, type=Path)
    arguments = parser.parse_args()
    baseline = json.loads(arguments.baseline.read_text())
    current = json.loads(arguments.current.read_text())
    regressions = compare(baseline, current)
    if regressions:
        for regression in regressions[:100]:
            print(regression)
        if len(regressions) > 100:
            print(f"... {len(regressions) - 100} additional regressions")
        return 1
    print(
        "renderer asset coverage baseline accepted: "
        f"{len(unresolved(current))} reviewed unresolved states"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
