#!/usr/bin/env python3
"""Bounded durable quorum-rotation runner for packaged Nostr visual audit."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
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


def newest_new_attempt(root: Path, before: set[Path]) -> Path | None:
    candidates = [
        path for path in root.iterdir()
        if path.is_dir() and path not in before
    ]
    return max(candidates, key=lambda path: path.stat().st_mtime_ns) \
        if candidates else None


def progress_signature(root: Path) -> tuple[tuple[str, int, int], ...]:
    """Describe durable child evidence without trusting console output."""
    values = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        stat = path.stat()
        values.append((str(path.relative_to(root)), stat.st_size,
                       stat.st_mtime_ns))
    return tuple(sorted(values))


def run_with_progress_deadline(
    command: list[str], *, cwd: Path, progress_root: Path,
    hard_timeout_seconds: float, progress_timeout_seconds: float,
    poll_seconds: float = 1.0,
) -> tuple[int, str | None]:
    """Run with finite total and inactivity deadlines backed by artifacts."""
    process = subprocess.Popen(command, cwd=cwd)
    started = time.monotonic()
    last_progress = started
    signature = progress_signature(progress_root)
    timeout_kind = None
    while process.poll() is None:
        now = time.monotonic()
        current = progress_signature(progress_root)
        if current != signature:
            signature = current
            last_progress = now
        if now - started >= hard_timeout_seconds:
            timeout_kind = "hard"
            break
        if now - last_progress >= progress_timeout_seconds:
            timeout_kind = "progress"
            break
        time.sleep(poll_seconds)
    if timeout_kind is None:
        return int(process.returncode), None
    process.terminate()
    try:
        process.wait(timeout=10.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    return 124, timeout_kind


def retain_attempt_timeout(
    attempt_root: Path, attempt_index: int, quorum: list[str],
    timeout_seconds: float, attempt_path: Path | None,
    *, timeout_kind: str = "hard", progress_timeout_seconds: float | None = None,
) -> Path:
    """Close a killed child attempt with durable truthful BLOCKED evidence."""
    if attempt_path is None:
        attempt_path = attempt_root / f"timeout-attempt-{attempt_index + 1}"
        attempt_path.mkdir()
    deadline_seconds = progress_timeout_seconds \
        if timeout_kind == "progress" else timeout_seconds
    error = (f"attempt exceeded deterministic {timeout_kind} deadline of "
             f"{deadline_seconds:g} seconds")
    for name in ("actions.jsonl", "correlated-frames.jsonl",
                 "visual-oracles.jsonl"):
        path = attempt_path / name
        if not path.exists():
            path.write_text("", encoding="utf-8")
    atomic_write_json(attempt_path / "coverage.json", {
        "schemaVersion": 1, "status": "BLOCKED",
        "missingRequiredCells": "unexecuted after attempt timeout",
    })
    atomic_write_json(attempt_path / "first-failure.json", {
        "error": f"TimeoutExpired: {error}",
        "classification": "attempt-deadline",
        "quorum": quorum,
        "timeoutKind": timeout_kind,
        "timeoutSeconds": deadline_seconds,
    })
    atomic_write_json(attempt_path / "verdict.json", {
        "schemaVersion": 1, "status": "BLOCKED", "failure": error,
    })
    run_path = attempt_path / "run.json"
    run = json.loads(run_path.read_text(encoding="utf-8")) \
        if run_path.is_file() else {"schemaVersion": 2}
    run.update({
        "status": "BLOCKED", "selectedQuorum": quorum,
        "attemptTimeoutSeconds": timeout_seconds,
    })
    atomic_write_json(run_path, run)
    write_report(
        attempt_path, "BLOCKED", error,
        report_path=attempt_path / "report.md",
    )
    return attempt_path


def retained_path(path: Path | None) -> str | None:
    if path is None:
        return None
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


class MinimizationBlocked(RuntimeError):
    """Candidate run could not decide whether prefix preserves failure."""


VOLATILE_FAILURE_KEYS = {
    "screenshot", "images", "manifestPath", "actualPixelCropPath",
    "expectedPixelCropPath", "minimizedReplayPath", "traceback",
    "completedEvidence",
}

NON_MINIMIZABLE_EXCEPTION_PREFIXES = (
    "InvalidSessionIdException:",
    "NoSuchWindowException:",
    "SessionNotCreatedException:",
)


def stable_failure_value(value):
    if isinstance(value, dict):
        return {
            key: stable_failure_value(child)
            for key, child in value.items()
            if key not in VOLATILE_FAILURE_KEYS and not key.endswith("Path")
        }
    if isinstance(value, list):
        return [stable_failure_value(child) for child in value]
    return value


def canonical_failure_identity(root: Path | None) -> dict[str, object] | None:
    if root is None:
        return None
    failure_path = root / "first-failure.json"
    if failure_path.is_file():
        failure = json.loads(failure_path.read_text(encoding="utf-8"))
        error = str(failure.get("error", ""))
        if error and not error.startswith(
            ("ActionLimitReached:", *NON_MINIMIZABLE_EXCEPTION_PREFIXES)
        ):
            return {
                "kind": "exception", "value": error,
                "sha256": hashlib.sha256(error.encode()).hexdigest(),
            }
    visual_path = root / "visual-failures.json"
    if visual_path.is_file():
        failures = json.loads(visual_path.read_text(encoding="utf-8"))
        if failures:
            value = stable_failure_value(failures[0])
            encoded = json.dumps(value, sort_keys=True, separators=(",", ":"))
            return {
                "kind": "visual-oracle", "value": value,
                "sha256": hashlib.sha256(encoded.encode()).hexdigest(),
            }
    screenshot_path = root / "screenshot-audit.json"
    if screenshot_path.is_file():
        report = json.loads(screenshot_path.read_text(encoding="utf-8"))
        failures = [
            finding for finding in report.get("findings", [])
            if finding.get("status") == "FAIL"
        ]
        if failures:
            value = stable_failure_value(failures[0])
            encoded = json.dumps(value, sort_keys=True, separators=(",", ":"))
            return {
                "kind": "screenshot-oracle", "value": value,
                "sha256": hashlib.sha256(encoded.encode()).hexdigest(),
            }
    return None


def read_action_stream(root: Path) -> list[dict[str, object]]:
    path = root / "actions.jsonl"
    if not path.is_file():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip()]


def stopped_at_action_limit(root: Path | None) -> bool:
    path = root / "first-failure.json" if root else None
    if not path or not path.is_file():
        return False
    value = json.loads(path.read_text(encoding="utf-8"))
    return str(value.get("error", "")).startswith("ActionLimitReached:")


def rejected_relays(root: Path | None) -> dict[str, list[str]]:
    """Read relays proven incompatible by retained production acknowledgements."""
    path = root / "first-failure.json" if root else None
    if not path or not path.is_file():
        return {}
    failure = json.loads(path.read_text(encoding="utf-8"))
    blocker = failure.get("infrastructureBlocker")
    if not isinstance(blocker, dict):
        return {}
    rejected: dict[str, list[str]] = {}
    publications = blocker.get("rejectedPublications", {})
    if not isinstance(publications, dict):
        return {}
    for peer_records in publications.values():
        if not isinstance(peer_records, list):
            continue
        for publication in peer_records:
            if not isinstance(publication, dict):
                continue
            intent = str(publication.get("intentId", "unknown"))
            for result in publication.get("results", []):
                if not isinstance(result, dict) or bool(result.get("ok")):
                    continue
                relay = str(result.get("relay", "")).rstrip("/")
                if relay and intent not in rejected.setdefault(relay, []):
                    rejected[relay].append(intent)
    return rejected


def minimize_prefix(
    action_count: int, target_sha256: str, run_candidate,
) -> tuple[int, list[dict[str, object]]]:
    """Binary-search smallest monotonic prefix retaining exact failure."""
    if action_count < 1:
        raise ValueError("prefix minimization needs at least one action")
    low, high = 0, action_count
    attempts: list[dict[str, object]] = []
    while low < high:
        middle = (low + high) // 2
        candidate = run_candidate(middle)
        attempts.append(candidate)
        status = candidate.get("status")
        if status == "BLOCKED":
            raise MinimizationBlocked(
                f"candidate prefix {middle} was infrastructure-blocked"
            )
        reproduced = (
            status == "FAIL" and
            (candidate.get("failureIdentity") or {}).get("sha256") ==
            target_sha256
        )
        if reproduced:
            high = middle
        else:
            low = middle + 1
    return low, attempts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8892)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--checkpoint", action="store_true")
    parser.add_argument("--seed", type=int, default=0xA0E20260812)
    parser.add_argument("--retry-budget", type=int, default=3)
    parser.add_argument(
        "--attempt-timeout-seconds", type=float, default=14400.0,
        help="hard wall-clock cap for one attempt",
    )
    parser.add_argument(
        "--progress-timeout-seconds", type=float, default=600.0,
        help="cap between durable audit artifact updates",
    )
    parser.add_argument("--viewport", type=parse_viewport, default=(1280, 900))
    parser.add_argument("--dpr", type=float, choices=(1.0, 2.0), default=1.0)
    parser.add_argument("--zoom", type=float, choices=(1.0, 2.0), default=1.0)
    parser.add_argument("--browser-argument", action="append", default=[])
    parser.add_argument("--audit-root", type=Path, default=AUDIT_ROOT)
    parser.add_argument("--report-root", type=Path, default=AUDIT_REPORT_ROOT)
    parser.add_argument(
        "--destination-id",
        help="reserve an exact durable aggregate directory and report suffix",
    )
    arguments = parser.parse_args()

    destination = allocate_audit_destination(
        arguments.audit_root, arguments.report_root, arguments.destination_id
    )
    configured = list(DEFAULT_RELAYS)
    initialize_run_ledger(
        destination, relays=",".join(configured), headed=arguments.headed,
        port=arguments.port, seed=arguments.seed,
        retry_budget=arguments.retry_budget,
        viewport=arguments.viewport, dpr=arguments.dpr,
        browser_arguments=arguments.browser_argument,
        zoom=arguments.zoom,
    )
    probe = probe_relay_pool(configured)
    atomic_write_json(destination.artifacts / "relay-probe.json", probe)
    healthy = [
        result["relay"] for result in probe["results"] if result["healthy"]
    ]
    quorums = [configured for _ in range(arguments.retry_budget + 1)]
    attempt_root = destination.artifacts / "attempts"
    attempt_reports = destination.artifacts / "attempt-reports"
    attempt_root.mkdir()
    attempt_reports.mkdir()
    attempts: list[dict[str, object]] = []
    maximum_attempts = min(arguments.retry_budget + 1, len(quorums))
    final_status = "BLOCKED"
    final_attempt_path: Path | None = None
    incompatible_relays: dict[str, list[str]] = {}
    skipped_quorums: list[dict[str, object]] = []

    for quorum in quorums:
        conflicts = sorted(set(quorum) & incompatible_relays.keys())
        if conflicts:
            skipped_quorums.append({
                "quorum": quorum,
                "reason": "contains-proven-incompatible-relay",
                "relays": conflicts,
            })
            continue
        if len(attempts) >= maximum_attempts:
            break
        attempt_index = len(attempts)
        command = [
            str(ROOT / "build-web" / "selenium-venv" / "bin" / "python"),
            str(ROOT / "tests" / "web" / "nostr_multiplayer_smoke_test.py"),
            "--port", str(arguments.port),
            "--seed", str(arguments.seed),
            "--retry-budget", "0",
            "--viewport", f"{arguments.viewport[0]}x{arguments.viewport[1]}",
            "--dpr", str(arguments.dpr),
            "--zoom", str(arguments.zoom),
            "--audit-root", str(attempt_root),
            "--report-root", str(attempt_reports),
        ]
        if arguments.headed:
            command.append("--headed")
        if arguments.checkpoint:
            command.append("--checkpoint")
        for browser_argument in arguments.browser_argument:
            command.append(f"--browser-argument={browser_argument}")
        before_attempts = set(attempt_root.iterdir())
        return_code, timeout_kind = run_with_progress_deadline(
            command, cwd=ROOT, progress_root=attempt_root,
            hard_timeout_seconds=arguments.attempt_timeout_seconds,
            progress_timeout_seconds=arguments.progress_timeout_seconds,
        )
        timed_out = timeout_kind is not None
        attempt_path = newest_new_attempt(attempt_root, before_attempts)
        if timed_out:
            attempt_path = retain_attempt_timeout(
                attempt_root, attempt_index, quorum,
                arguments.attempt_timeout_seconds, attempt_path,
                timeout_kind=str(timeout_kind),
                progress_timeout_seconds=arguments.progress_timeout_seconds,
            )
        final_attempt_path = attempt_path
        verdict_path = attempt_path / "verdict.json" if attempt_path else None
        verdict = json.loads(verdict_path.read_text()) \
            if verdict_path and verdict_path.is_file() else {
                "status": "BLOCKED", "failure": "attempt verdict missing",
            }
        status = str(verdict.get("status", "BLOCKED"))
        attempts.append({
            "attempt": attempt_index + 1,
            "quorum": quorum,
            "exitCode": return_code,
            "timedOut": timed_out,
            "status": status,
            "artifactPath": retained_path(attempt_path),
        })
        final_status = status
        if status == "BLOCKED":
            for relay, intents in rejected_relays(attempt_path).items():
                retained = incompatible_relays.setdefault(relay, [])
                retained.extend(
                    intent for intent in intents if intent not in retained
                )
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
        "incompatibleRelays": incompatible_relays,
        "skippedQuorums": skipped_quorums,
        "finalStatus": final_status,
    })
    minimization: dict[str, object] | None = None
    if final_status == "FAIL" and final_attempt_path is not None:
        target_identity = canonical_failure_identity(final_attempt_path)
        original_actions = read_action_stream(final_attempt_path)
        minimization_root = destination.artifacts / "minimization"
        minimization_reports = destination.artifacts / "minimization-reports"
        minimization_root.mkdir()
        minimization_reports.mkdir()
        candidate_paths: dict[int, Path] = {}
        retained_candidate_runs: list[dict[str, object]] = []

        def run_candidate(limit: int) -> dict[str, object]:
            before = set(minimization_root.iterdir())
            quorum = list(attempts[-1].get("quorum", []))
            command = [
                str(ROOT / "build-web" / "selenium-venv" / "bin" / "python"),
                str(ROOT / "tests" / "web" /
                    "nostr_multiplayer_smoke_test.py"),
                "--port", str(arguments.port),
                "--seed", str(arguments.seed), "--retry-budget", "0",
                "--viewport",
                f"{arguments.viewport[0]}x{arguments.viewport[1]}",
                "--dpr", str(arguments.dpr), "--zoom", str(arguments.zoom),
                "--action-limit", str(limit),
                "--audit-root", str(minimization_root),
                "--report-root", str(minimization_reports),
            ]
            if arguments.headed:
                command.append("--headed")
            for browser_argument in arguments.browser_argument:
                command.append(f"--browser-argument={browser_argument}")
            return_code, timeout_kind = run_with_progress_deadline(
                command, cwd=ROOT, progress_root=minimization_root,
                hard_timeout_seconds=arguments.attempt_timeout_seconds,
                progress_timeout_seconds=arguments.progress_timeout_seconds,
            )
            timed_out = timeout_kind is not None
            new_paths = [path for path in minimization_root.iterdir()
                         if path.is_dir() and path not in before]
            candidate_path = max(
                new_paths, key=lambda path: path.stat().st_mtime_ns
            ) if new_paths else None
            verdict_path = candidate_path / "verdict.json" \
                if candidate_path else None
            if timed_out:
                candidate_path = retain_attempt_timeout(
                    minimization_root, len(retained_candidate_runs), quorum,
                    arguments.attempt_timeout_seconds, candidate_path,
                )
                verdict_path = candidate_path / "verdict.json"
            verdict = json.loads(verdict_path.read_text(encoding="utf-8")) \
                if verdict_path and verdict_path.is_file() else {
                    "status": "BLOCKED",
                }
            status = str(verdict.get("status", "BLOCKED"))
            if status == "BLOCKED" and stopped_at_action_limit(candidate_path):
                status = "NOT_REPRODUCED"
            identity = canonical_failure_identity(candidate_path)
            if candidate_path:
                candidate_paths[limit] = candidate_path
            result = {
                "actionLimit": limit, "status": status,
                "exitCode": return_code, "timedOut": timed_out,
                "failureIdentity": identity,
                "artifactPath": retained_path(candidate_path),
            }
            retained_candidate_runs.append(result)
            return result

        if not target_identity:
            minimization = {
                "schemaVersion": 1, "status": "BLOCKED",
                "blocker": "original FAIL has no stable retained identity",
                "originalActionCount": len(original_actions),
            }
        elif not original_actions:
            minimization = {
                "schemaVersion": 1, "status": "PASS",
                "preservedFailure": True, "minimumActionCount": 0,
                "originalActionCount": 0,
                "failureIdentity": target_identity,
                "candidateRuns": [], "actions": [],
            }
        else:
            try:
                minimum, candidate_runs = minimize_prefix(
                    len(original_actions), str(target_identity["sha256"]),
                    run_candidate,
                )
                proof_path = candidate_paths.get(minimum, final_attempt_path)
                proof_identity = canonical_failure_identity(proof_path)
                preserved = (
                    proof_identity is not None and
                    proof_identity.get("sha256") == target_identity["sha256"]
                )
                if not preserved:
                    raise MinimizationBlocked(
                        "minimum prefix lacks retained matching failure proof"
                    )
                minimized_actions = read_action_stream(proof_path)[:minimum]
                minimization = {
                    "schemaVersion": 1, "status": "PASS",
                    "kind": "verified-prefix-minimization",
                    "preservedFailure": True,
                    "failureIdentity": target_identity,
                    "originalActionCount": len(original_actions),
                    "minimumActionCount": minimum,
                    "proofArtifactPath": retained_path(proof_path),
                    "candidateRuns": candidate_runs,
                    "actions": minimized_actions,
                }
            except MinimizationBlocked as error:
                minimization = {
                    "schemaVersion": 1, "status": "BLOCKED",
                    "blocker": str(error),
                    "failureIdentity": target_identity,
                    "originalActionCount": len(original_actions),
                    "candidateRuns": retained_candidate_runs,
                }
        atomic_write_json(
            destination.artifacts / "minimized-replay.json", minimization
        )
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
        "minimizationStatus": (
            minimization.get("status") if minimization else "NOT_APPLICABLE"
        ),
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
