#!/usr/bin/env python3
"""Run comprehensive Make stages while optionally hiding successful output."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile


STAGES = (
    "test",
    "web-build",
    "web-tests-only",
    "audit-browser-risk-spike-only",
    "audit-nostr-multiplayer",
)


def run_stage(make: str, stage: str, problems_only: bool) -> int:
    command = [make, stage]
    if not problems_only:
        return subprocess.run(command, check=False).returncode

    with tempfile.TemporaryFile(mode="w+t", encoding="utf-8") as output:
        result = subprocess.run(
            command,
            check=False,
            stdout=output,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode == 0:
            return 0
        output.seek(0)
        sys.stderr.write(
            f"check-all: {stage} failed with exit code {result.returncode}\n"
        )
        sys.stderr.write(output.read())
        return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--make", default="make")
    parser.add_argument("--problems-only", action="store_true")
    arguments = parser.parse_args()
    for stage in STAGES:
        status = run_stage(arguments.make, stage, arguments.problems_only)
        if status != 0:
            return status
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
