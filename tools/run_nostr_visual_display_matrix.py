#!/usr/bin/env python3
"""Run and retain packaged Nostr visual display/renderer matrix."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "web"))

from nostr_multiplayer_smoke_test import (  # noqa: E402
    AUDIT_REPORT_ROOT,
    AUDIT_ROOT,
    allocate_audit_destination,
    atomic_write_json,
    parse_viewport,
    write_report,
)


def display_cases(include_headed: bool) -> list[dict[str, object]]:
    cases = [
        {"id": "core-1280x900", "viewport": (1280, 900), "dpr": 1.0,
         "zoom": 1.0, "headed": False, "required": True,
         "browserArguments": []},
        {"id": "wide-1440x900", "viewport": (1440, 900), "dpr": 1.0,
         "zoom": 1.0, "headed": False, "required": True,
         "browserArguments": []},
        {"id": "square-1000x1000", "viewport": (1000, 1000), "dpr": 1.0,
         "zoom": 1.0, "headed": False, "required": True,
         "browserArguments": []},
        {"id": "retina-dpr2", "viewport": (1280, 900), "dpr": 2.0,
         "zoom": 1.0, "headed": False, "required": True,
         "browserArguments": []},
        {"id": "maximum-zoom", "viewport": (1280, 900), "dpr": 1.0,
         "zoom": 2.0, "headed": False, "required": True,
         "browserArguments": []},
        {"id": "swiftshader-webgl", "viewport": (1280, 900), "dpr": 1.0,
         "zoom": 1.0, "headed": False, "required": True,
         "browserArguments": ["--use-angle=swiftshader"]},
    ]
    headed = {
        "id": "headed-chrome", "viewport": (1280, 900), "dpr": 1.0,
        "zoom": 1.0, "headed": True, "required": False,
        "browserArguments": [],
    }
    if not include_headed:
        headed.update({
            "status": "BLOCKED", "executed": False,
            "blocker": (
                "headed execution not enabled; background-safe environment "
                "support was not declared"
            ),
        })
    cases.append(headed)
    return cases


def matrix_status(cases: list[dict[str, object]]) -> str:
    required = [case for case in cases if case.get("required")]
    if any(case.get("status") == "FAIL" and case.get("executed", True)
           for case in cases):
        return "FAIL"
    if any(case.get("status") != "PASS" for case in required):
        return "BLOCKED"
    return "PASS"


def newest_new_directory(root: Path, before: set[Path]) -> Path | None:
    candidates = [path for path in root.iterdir()
                  if path.is_dir() and path not in before]
    return max(candidates, key=lambda path: path.stat().st_mtime_ns) \
        if candidates else None


def retained_path(path: Path | None) -> str | None:
    if path is None:
        return None
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def renderer_evidence(child: Path | None) -> dict[str, object] | None:
    if child is None:
        return None
    attempts_path = child / "attempts.json"
    if not attempts_path.is_file():
        return None
    attempts = json.loads(attempts_path.read_text(encoding="utf-8")).get(
        "attempts", []
    )
    if not attempts:
        return None
    artifact_value = attempts[-1].get("artifactPath")
    if not artifact_value:
        return None
    artifact = Path(str(artifact_value))
    if not artifact.is_absolute():
        artifact = ROOT / artifact
    evidence_path = artifact / "evidence.json"
    if not evidence_path.is_file():
        return None
    browser = json.loads(evidence_path.read_text(encoding="utf-8")).get(
        "browser", {}
    )
    return {
        "host": browser.get("hostRenderer"),
        "join": browser.get("joinRenderer"),
        "arguments": browser.get("arguments", []),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--relays")
    parser.add_argument("--port", type=int, default=8892)
    parser.add_argument("--seed", type=int, default=0xA0E20260812)
    parser.add_argument("--retry-budget", type=int, default=3)
    parser.add_argument("--include-headed", action="store_true")
    parser.add_argument("--audit-root", type=Path, default=AUDIT_ROOT)
    parser.add_argument("--report-root", type=Path, default=AUDIT_REPORT_ROOT)
    arguments = parser.parse_args()

    destination = allocate_audit_destination(
        arguments.audit_root, arguments.report_root
    )
    cells_root = destination.artifacts / "cells"
    reports_root = destination.artifacts / "cell-reports"
    cells_root.mkdir(parents=True)
    reports_root.mkdir(parents=True)
    cases = display_cases(arguments.include_headed)
    ledger = {
        "schemaVersion": 1, "kind": "nostr-visual-display-matrix",
        "status": "RUNNING", "startedUtc": datetime.now(timezone.utc).isoformat(),
        "artifactPath": str(destination.artifacts),
        "reportPath": str(destination.report), "seed": arguments.seed,
        "cameraModes": ["centered", "edge-clamped", "panning"],
        "cases": cases,
    }
    atomic_write_json(destination.artifacts / "run.json", ledger)
    atomic_write_json(destination.artifacts / "matrix.json", ledger)
    write_report(
        destination.artifacts, "RUNNING",
        "Display matrix destination declared before browser launch.",
        report_path=destination.report,
    )

    for case_index, case in enumerate(cases):
        if case.get("executed") is False:
            continue
        before = set(cells_root.iterdir())
        viewport = case["viewport"]
        command = [
            str(ROOT / "build-web" / "selenium-venv" / "bin" / "python"),
            str(ROOT / "tools" / "run_nostr_visual_audit.py"),
            "--port", str(arguments.port + case_index),
            "--seed", str(arguments.seed),
            "--retry-budget", str(arguments.retry_budget),
            "--viewport", f"{viewport[0]}x{viewport[1]}",
            "--dpr", str(case["dpr"]), "--zoom", str(case["zoom"]),
            "--audit-root", str(cells_root),
            "--report-root", str(reports_root),
        ]
        if arguments.relays:
            command.extend(["--relays", arguments.relays])
        if case["headed"]:
            command.append("--headed")
        for browser_argument in case["browserArguments"]:
            command.append(f"--browser-argument={browser_argument}")
        completed = subprocess.run(command, cwd=ROOT, check=False)
        child = newest_new_directory(cells_root, before)
        verdict_path = child / "verdict.json" if child else None
        verdict = json.loads(verdict_path.read_text(encoding="utf-8")) \
            if verdict_path and verdict_path.is_file() else {
                "status": "BLOCKED", "failure": "child verdict missing",
            }
        renderer = renderer_evidence(child)
        status = verdict.get("status", "BLOCKED")
        if case["id"] == "swiftshader-webgl" and status == "PASS":
            identities = " ".join(
                str((renderer or {}).get(peer, {}))
                for peer in ("host", "join")
            ).lower()
            if "swiftshader" not in identities:
                status = "BLOCKED"
                case["blocker"] = (
                    "Chrome did not prove SwiftShader on production canvas"
                )
        case.update({
            "executed": True, "exitCode": completed.returncode,
            "status": status, "rendererEvidence": renderer,
            "artifactPath": retained_path(child),
        })
        atomic_write_json(destination.artifacts / "matrix.json", {
            **ledger, "cases": cases, "status": matrix_status(cases),
        })

    status = matrix_status(cases)
    completed_utc = datetime.now(timezone.utc).isoformat()
    final = {**ledger, "status": status, "completedUtc": completed_utc,
             "cases": cases}
    atomic_write_json(destination.artifacts / "matrix.json", final)
    atomic_write_json(destination.artifacts / "verdict.json", {
        "schemaVersion": 1, "status": status,
        "requiredCaseCount": sum(bool(case.get("required")) for case in cases),
        "blockedOptionalCases": [case["id"] for case in cases
                                 if not case.get("required") and
                                 case.get("status") == "BLOCKED"],
        "matrix": "matrix.json",
    })
    atomic_write_json(destination.artifacts / "run.json", final)
    write_report(
        destination.artifacts, status,
        "Display/renderer matrix complete. See `matrix.json` and retained "
        "per-cell artifacts.", report_path=destination.report,
    )
    print(f"Nostr visual display matrix {status.lower()}: "
          f"{destination.artifacts}")
    return 0 if status == "PASS" else 1 if status == "FAIL" else 2


if __name__ == "__main__":
    raise SystemExit(main())
