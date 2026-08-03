#!/usr/bin/env python3
"""Run one deterministic renderer capture, then build overlap review bundle."""

from __future__ import annotations

import argparse
import os
import resource
import subprocess
import sys
from pathlib import Path


def is_game_process_name(name: str) -> bool:
    """Match executable names, never incidental command-line arguments."""
    return name.strip() in {"aoe_reconstruction", "AoE Archaeology"}


def game_processes() -> list[str]:
    result = subprocess.run(
        ["ps", "-axo", "pid=,ucomm="], check=True,
        capture_output=True, text=True,
    )
    found = []
    for line in result.stdout.splitlines():
        pid, separator, name = line.strip().partition(" ")
        if separator and pid.isdigit() and is_game_process_name(name):
            found.append(line.strip())
    return found


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--tick", type=int, default=0)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument(
        "--manifest-only", action="store_true",
        help="capture renderer inputs and manifest without building a review bundle",
    )
    args = parser.parse_args(argv)
    if args.tick < 0:
        parser.error("--tick must be non-negative")
    running = game_processes()
    if running:
        parser.error("game process already running: " + running[0])
    soft_limit, _ = resource.getrlimit(resource.RLIMIT_NOFILE)
    descriptor_count = len(list(Path("/dev/fd").iterdir()))
    if descriptor_count * 2 >= soft_limit:
        parser.error(
            f"descriptor preflight failed: {descriptor_count} open, soft limit {soft_limit}"
        )
    args.capture_dir.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    environment.setdefault("SDL_VIDEODRIVER", "dummy")
    environment.setdefault("SDL_AUDIODRIVER", "dummy")
    environment.setdefault("SDL_RENDER_DRIVER", "software")
    environment.update({
        "AOE_MAIN_MENU": "0",
        "AOE_AUDIT_ANY_MAP_SIZE": "1",
        "AOE_SCENARIO_PATH": str(args.scenario.resolve()),
        "AOE_OVERLAP_CAPTURE_DIR": str(args.capture_dir.resolve()),
        "AOE_OVERLAP_CAPTURE_TICK": str(args.tick),
        "AOE_OVERLAP_CAPTURE_EXIT": "1",
    })
    try:
        completed = subprocess.run(
            [str(args.executable.resolve())], env=environment,
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, timeout=args.timeout,
        )
    except subprocess.TimeoutExpired as error:
        parser.error(f"capture timed out after {args.timeout}s: {error}")
    if completed.returncode:
        parser.error(
            f"capture process exited {completed.returncode}: "
            + completed.stdout[-1000:]
        )
    manifest = args.capture_dir / "manifest.json"
    if not manifest.is_file():
        parser.error("renderer exited without manifest.json")
    if args.manifest_only:
        return 0
    reviewer = Path(__file__).with_name("batch_visual_overlap_audit.py")
    review = subprocess.run([
        sys.executable, str(reviewer), str(manifest),
        "--output-dir", str(args.output_dir),
    ])
    return review.returncode


if __name__ == "__main__":
    sys.exit(main())
