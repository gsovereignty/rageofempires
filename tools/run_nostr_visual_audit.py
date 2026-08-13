#!/usr/bin/env python3
"""Bounded durable quorum-rotation runner for packaged Nostr visual audit."""

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
    DEFAULT_RELAYS,
    allocate_audit_destination,
    atomic_write_json,
    initialize_run_ledger,
    parse_viewport,
    probe_relay_pool,
    write_report,
)


def rotating_quorums(relays: list[str], size: int = 3) -> list[list[str]]:
    """Return deterministic distinct windows, preserving configured order."""
    if size < 2:
        raise ValueError("relay quorum size must be at least two")
    unique = list(dict.fromkeys(relays))
    if len(unique) < 2:
        return []
    width = min(size, len(unique))
    quorums: list[list[str]] = []
    for start in range(len(unique)):
        quorum = [unique[(start + offset) % len(unique)] for offset in range(width)]
        if quorum not in quorums:
            quorums.append(quorum)
    return quorums


def latest_attempt(root: Path) -> Path | None:
    candidates = [path for path in root.iterdir() if path.is_dir()]
    return max(candidates, key=lambda path: path.stat().st_mtime_ns) \
        if candidates else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--relays")
    parser.add_argument("--port", type=int, default=8892)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--checkpoint", action="store_true")
    parser.add_argument("--seed", type=int, default=0xA0E20260812)
    parser.add_argument("--retry-budget", type=int, default=3)
    parser.add_argument("--viewport", type=parse_viewport, default=(1280, 900))
    parser.add_argument("--dpr", type=float, choices=(1.0, 2.0), default=1.0)
    parser.add_argument("--audit-root", type=Path, default=AUDIT_ROOT)
    parser.add_argument("--report-root", type=Path, default=AUDIT_REPORT_ROOT)
    arguments = parser.parse_args()

    destination = allocate_audit_destination(
        arguments.audit_root, arguments.report_root
    )
    configured = arguments.relays.split(",") \
        if arguments.relays else list(DEFAULT_RELAYS)
    initialize_run_ledger(
        destination, relays=",".join(configured), headed=arguments.headed,
        port=arguments.port, seed=arguments.seed,
        retry_budget=arguments.retry_budget,
        viewport=arguments.viewport, dpr=arguments.dpr,
    )
    probe = probe_relay_pool(configured)
    atomic_write_json(destination.artifacts / "relay-probe.json", probe)
    healthy = [
        result["relay"] for result in probe["results"] if result["healthy"]
    ]
    quorums = rotating_quorums(healthy)
    attempt_root = destination.artifacts / "attempts"
    attempt_reports = destination.artifacts / "attempt-reports"
    attempt_root.mkdir()
    attempt_reports.mkdir()
    attempts: list[dict[str, object]] = []
    maximum_attempts = min(arguments.retry_budget + 1, len(quorums))
    final_status = "BLOCKED"

    for attempt_index, quorum in enumerate(quorums[:maximum_attempts]):
        command = [
            str(ROOT / "build-web" / "selenium-venv" / "bin" / "python"),
            str(ROOT / "tests" / "web" / "nostr_multiplayer_smoke_test.py"),
            "--port", str(arguments.port),
            "--relays", ",".join(quorum),
            "--seed", str(arguments.seed),
            "--retry-budget", "0",
            "--viewport", f"{arguments.viewport[0]}x{arguments.viewport[1]}",
            "--dpr", str(arguments.dpr),
            "--audit-root", str(attempt_root),
            "--report-root", str(attempt_reports),
        ]
        if arguments.headed:
            command.append("--headed")
        if arguments.checkpoint:
            command.append("--checkpoint")
        completed = subprocess.run(command, cwd=ROOT, check=False)
        attempt_path = latest_attempt(attempt_root)
        verdict_path = attempt_path / "verdict.json" if attempt_path else None
        verdict = json.loads(verdict_path.read_text()) \
            if verdict_path and verdict_path.is_file() else {
                "status": "BLOCKED", "failure": "attempt verdict missing",
            }
        status = str(verdict.get("status", "BLOCKED"))
        attempts.append({
            "attempt": attempt_index + 1,
            "quorum": quorum,
            "exitCode": completed.returncode,
            "status": status,
            "artifactPath": str(attempt_path.relative_to(ROOT))
                if attempt_path else None,
        })
        final_status = status
        if status in {"PASS", "FAIL"}:
            break

    if not attempts:
        attempts.append({
            "attempt": 0, "quorum": [], "status": "BLOCKED",
            "failure": "fewer than two healthy configured relays",
        })
    atomic_write_json(destination.artifacts / "attempts.json", {
        "schemaVersion": 1,
        "seed": arguments.seed,
        "retryBudget": arguments.retry_budget,
        "attempts": attempts,
        "finalStatus": final_status,
    })
    ledger = json.loads((destination.artifacts / "run.json").read_text())
    ledger.update({
        "status": final_status,
        "selectedQuorum": attempts[-1].get("quorum", []),
        "attemptCount": len(attempts),
        "finalizedUtc": datetime.now(timezone.utc).isoformat(),
    })
    atomic_write_json(destination.artifacts / "run.json", ledger)
    atomic_write_json(destination.artifacts / "verdict.json", {
        "schemaVersion": 1,
        "status": final_status,
        "attemptCount": len(attempts),
        "attemptLedger": "attempts.json",
    })
    write_report(
        destination.artifacts, final_status,
        f"Bounded relay runner completed {len(attempts)} attempt(s). "
        "See `attempts.json` and retained per-attempt artifacts.",
        report_path=destination.report,
    )
    print(
        f"Nostr visual audit {final_status.lower()}: {destination.artifacts}"
    )
    return 0 if final_status == "PASS" else 1 if final_status == "FAIL" else 2


if __name__ == "__main__":
    raise SystemExit(main())
