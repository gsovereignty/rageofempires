#!/usr/bin/env python3
"""Validate overlap decisions and compare or update an explicit baseline."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

DECISIONS = {"bug", "intentional", "uncertain"}


def load_object(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read {label} {path}: {error}") from error
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        raise ValueError(f"{label} must be a schema-version-1 object")
    return value


def fingerprint(case: dict[str, object]) -> str:
    stable = {
        "status": case.get("status"),
        "blocked_reason": case.get("blocked_reason"),
        "metadata": case.get("metadata", {}),
        "detector": case.get("detector"),
    }
    encoded = json.dumps(stable, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def validate_report(value: dict[str, object]) -> dict[str, dict[str, object]]:
    cases = value.get("cases")
    if not isinstance(cases, list):
        raise ValueError("report cases must be an array")
    indexed: dict[str, dict[str, object]] = {}
    for case in cases:
        if not isinstance(case, dict) or not isinstance(case.get("id"), str):
            raise ValueError("every report case must have a string id")
        identifier = case["id"]
        if identifier in indexed:
            raise ValueError(f"duplicate report case: {identifier}")
        indexed[identifier] = case
    return indexed


def validate_decisions(value: dict[str, object]) -> dict[str, str]:
    raw = value.get("decisions")
    if not isinstance(raw, list):
        raise ValueError("decisions must be an array")
    indexed: dict[str, str] = {}
    for item in raw:
        if not isinstance(item, dict) or not isinstance(item.get("id"), str):
            raise ValueError("every decision must have a string id")
        decision = item.get("decision")
        if decision not in DECISIONS:
            raise ValueError(
                f"decision for {item['id']} must be bug, intentional, or uncertain"
            )
        if item["id"] in indexed:
            raise ValueError(f"duplicate decision: {item['id']}")
        indexed[item["id"]] = decision
    return indexed


def compare(
    report: dict[str, object], baseline: dict[str, object] | None
) -> dict[str, object]:
    cases = validate_report(report)
    raw_entries = {} if baseline is None else baseline.get("entries", {})
    if not isinstance(raw_entries, dict):
        raise ValueError("baseline entries must be an object")
    new: list[str] = []
    changed: list[str] = []
    unresolved: list[str] = []
    for identifier, case in sorted(cases.items()):
        entry = raw_entries.get(identifier)
        if not isinstance(entry, dict):
            new.append(identifier)
            continue
        if entry.get("fingerprint") != fingerprint(case):
            changed.append(identifier)
        if entry.get("decision") == "uncertain":
            unresolved.append(identifier)
    missing = sorted(set(raw_entries) - set(cases))
    return {
        "schema_version": 1,
        "new": new,
        "changed": changed,
        "missing": missing,
        "unresolved": unresolved,
        "summary": {key: len(value) for key, value in (
            ("new", new), ("changed", changed),
            ("missing", missing), ("unresolved", unresolved),
        )},
    }


def update_baseline(
    report: dict[str, object], decisions: dict[str, str]
) -> dict[str, object]:
    cases = validate_report(report)
    eligible = {
        identifier: case for identifier, case in cases.items()
        if case.get("status") != "blocked"
    }
    unknown = sorted(set(decisions) - set(eligible))
    undecided = sorted(set(eligible) - set(decisions))
    if unknown:
        raise ValueError(f"decisions reference unknown cases: {', '.join(unknown)}")
    if undecided:
        raise ValueError(f"decisions missing cases: {', '.join(undecided)}")
    return {
        "schema_version": 1,
        "entries": {
            identifier: {
                "decision": decisions[identifier],
                "fingerprint": fingerprint(case),
            }
            for identifier, case in sorted(eligible.items())
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--decisions", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = load_object(args.report, "report")
        if args.decisions:
            decisions = validate_decisions(load_object(args.decisions, "decisions"))
            result = update_baseline(report, decisions)
        else:
            baseline = load_object(args.baseline, "baseline") if args.baseline else None
            result = compare(report, baseline)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
