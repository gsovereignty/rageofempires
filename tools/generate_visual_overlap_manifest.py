#!/usr/bin/env python3
"""Capture a scenario/tick corpus and build one visual-overlap review bundle."""

from __future__ import annotations

import argparse
import glob
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


SAFE_ID = re.compile(r"[^A-Za-z0-9._-]+")


def parse_ticks(value: str) -> list[int]:
    """Parse a comma-separated, unique, non-negative tick schedule."""
    try:
        ticks = [int(part.strip()) for part in value.split(",") if part.strip()]
    except ValueError as error:
        raise argparse.ArgumentTypeError("ticks must be comma-separated integers") from error
    if not ticks:
        raise argparse.ArgumentTypeError("at least one tick is required")
    if any(tick < 0 for tick in ticks):
        raise argparse.ArgumentTypeError("ticks must be non-negative")
    return list(dict.fromkeys(ticks))


def resolve_scenarios(patterns: list[str]) -> list[Path]:
    """Resolve literal paths and glob patterns into a sorted unique corpus."""
    resolved: dict[str, Path] = {}
    for pattern in patterns:
        matches = [Path(path) for path in glob.glob(pattern)]
        if not matches and Path(pattern).is_file():
            matches = [Path(pattern)]
        for path in matches:
            if path.is_file():
                resolved[str(path.resolve())] = path.resolve()
    return [resolved[key] for key in sorted(resolved)]


def case_prefix(scenario: Path, tick: int) -> str:
    stem = SAFE_ID.sub("-", scenario.stem).strip("-")
    return f"{stem}-t{tick}"


def merge_manifest(
    master_cases: list[dict[str, object]], manifest_path: Path,
    relative_capture: Path, prefix: str,
) -> None:
    """Append one renderer manifest with globally stable IDs and paths."""
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("schema_version") != 1 or not isinstance(manifest.get("cases"), list):
        raise ValueError(f"invalid renderer manifest: {manifest_path}")
    for raw_case in manifest["cases"]:
        if not isinstance(raw_case, dict) or not isinstance(raw_case.get("id"), str):
            raise ValueError(f"invalid renderer case: {manifest_path}")
        case = dict(raw_case)
        case["id"] = f"{prefix}-{raw_case['id']}"
        for key in ("actual", "terrain", "sprite"):
            if key in case:
                case[key] = str(relative_capture / str(case[key]))
        master_cases.append(case)


def blocked_case(scenario: Path, tick: int, prefix: str, reason: str) -> dict[str, object]:
    """Represent an unavailable capture without dropping corpus coverage."""
    return {
        "id": f"{prefix}-capture",
        "blocked_reason": reason,
        "metadata": {"scenario": scenario.name, "tick": tick},
    }


def run_corpus(
    executable: Path, scenarios: list[Path], ticks: list[int],
    output_dir: Path, timeout: int, resume: bool = False,
) -> int:
    """Capture full corpus, write master manifest, and build review gallery."""
    if resume:
        if not output_dir.is_dir():
            raise ValueError(f"resume output directory does not exist: {output_dir}")
    else:
        output_dir.mkdir(parents=True, exist_ok=False)
    captures = output_dir / "captures"
    captures.mkdir(exist_ok=resume)
    capture_tool = Path(__file__).with_name("capture_visual_overlap.py")
    batch_tool = Path(__file__).with_name("batch_visual_overlap_audit.py")
    cases: list[dict[str, object]] = []
    total = len(scenarios) * len(ticks)
    completed = 0
    for scenario in scenarios:
        for tick in ticks:
            prefix = case_prefix(scenario, tick)
            capture_dir = captures / prefix
            manifest_path = capture_dir / "manifest.json"
            if resume and manifest_path.is_file():
                merge_manifest(
                    cases, manifest_path, Path("captures") / prefix, prefix,
                )
                completed += 1
                print(
                    f"reused {completed}/{total}: {scenario.name} tick {tick}",
                    flush=True,
                )
                continue
            if resume and capture_dir.exists():
                shutil.rmtree(capture_dir)
            command = [
                sys.executable, str(capture_tool), str(executable), str(scenario),
                "--capture-dir", str(capture_dir), "--output-dir", str(output_dir / "unused"),
                "--tick", str(tick), "--timeout", str(timeout), "--manifest-only",
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                detail = (result.stderr or result.stdout).strip().splitlines()
                reason = detail[-1] if detail else f"capture exited {result.returncode}"
                cases.append(blocked_case(scenario, tick, prefix, reason[-500:]))
                status = "blocked"
            else:
                merge_manifest(
                    cases, capture_dir / "manifest.json",
                    Path("captures") / prefix, prefix,
                )
                status = "captured"
            completed += 1
            print(
                f"{status} {completed}/{total}: {scenario.name} tick {tick}",
                flush=True,
            )
    master = {"schema_version": 1, "cases": cases}
    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(master, indent=2, sort_keys=True) + "\n")
    review = subprocess.run([
        sys.executable, str(batch_tool), str(manifest_path),
        "--output-dir", str(output_dir),
    ])
    return review.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("scenarios", nargs="+", help="scenario paths or glob patterns")
    parser.add_argument("--ticks", type=parse_ticks, default=parse_ticks("0"))
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=120, help="seconds per capture")
    parser.add_argument(
        "--resume", action="store_true",
        help="reuse completed captures and retry incomplete slots in output directory",
    )
    args = parser.parse_args(argv)
    if not args.executable.is_file():
        parser.error(f"executable not found: {args.executable}")
    scenarios = resolve_scenarios(args.scenarios)
    if not scenarios:
        parser.error("no scenario files matched")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        return run_corpus(
            args.executable.resolve(), scenarios, args.ticks,
            args.output_dir.resolve(), args.timeout, args.resume,
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    sys.exit(main())
