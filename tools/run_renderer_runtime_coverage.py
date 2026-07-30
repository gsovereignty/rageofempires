#!/usr/bin/env python3
"""Exercise deterministic renderer scenarios and merge fallback telemetry."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


CASES = (
    ("demo.scenario", 0),
    ("visual-audit.scenario", 0),
    ("combat-pose-audit.scenario", 0),
    ("combat-pose-audit.scenario", 8),
    ("movement-gait-audit.scenario", 4),
    ("villager-work-audit.scenario", 8),
    ("villager-repair-audit.scenario", 8),
    ("monk-conversion-audit.scenario", 12),
    ("monk-relic-healing-audit.scenario", 12),
    ("naval-fishing-audit.scenario", 12),
    ("naval-combat-transport-audit.scenario", 12),
    ("fire-demolition-ship-audit.scenario", 12),
    ("unique-naval-units-audit.scenario", 4),
    ("castle-unique-units-audit.scenario", 4),
    ("eastern-castle-unique-units-audit.scenario", 4),
    ("expanded-civilizations-audit.scenario", 4),
    ("conquerors-civilizations-audit.scenario", 4),
    ("final-civilizations-audit.scenario", 4),
    ("damage-audit.scenario", 0),
    ("damage-audit.scenario", 8),
    ("rubble-audit.scenario", 8),
    ("farm-render-audit.scenario", 4),
    ("palisade-gate-render-audit.scenario", 4),
    ("stone-gate-render-audit.scenario", 4),
)


def run_case(
    executable: Path,
    scenario: Path,
    tick: int,
    directory: Path,
) -> list[dict]:
    stem = f"{scenario.stem}-{tick}"
    report = directory / f"{stem}.json"
    capture = directory / f"{stem}.bmp"
    environment = os.environ.copy()
    environment.update(
        {
            "SDL_VIDEO_DRIVER": "dummy",
            "AOE_WINDOW_SIZE": "800x600",
            "AOE_SCENARIO_PATH": str(scenario),
            "AOE_SCREENSHOT_PATH": str(capture),
            "AOE_SCREENSHOT_TICK": str(tick),
            "AOE_EXIT_AFTER_SCREENSHOT": "1",
            "AOE_RENDER_FALLBACK_REPORT": str(report),
        }
    )
    completed = subprocess.run(
        [str(executable)],
        env=environment,
        capture_output=True,
        text=True,
        timeout=90,
        check=False,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        raise RuntimeError(
            f"{scenario.name}@{tick} exited {completed.returncode}: "
            f"{detail[:800]}"
        )
    if not capture.is_file() or capture.stat().st_size == 0:
        raise RuntimeError(f"{scenario.name}@{tick} produced no frame")
    if not report.is_file():
        return []
    document = json.loads(report.read_text())
    if document.get("schema") != "aoe-runtime-render-fallback-v1":
        raise RuntimeError(f"{scenario.name}@{tick} has wrong telemetry schema")
    return document.get("events", [])


def merge_events(event_groups: list[list[dict]]) -> list[dict]:
    merged: dict[str, dict] = {}
    comparison_fields = ("render_state", "requested_asset", "status", "reason")
    for events in event_groups:
        for event in events:
            key = event["stable_render_state_key"]
            existing = merged.get(key)
            if existing is None:
                merged[key] = event
                continue
            if any(existing[field] != event[field] for field in comparison_fields):
                raise RuntimeError(
                    f"runtime state {key} produced conflicting telemetry"
                )
    return [merged[key] for key in sorted(merged)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    executable = args.executable.resolve()
    resources = args.resources.resolve()
    if not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    for name, _ in CASES:
        if not (resources / name).is_file():
            parser.error(f"scenario does not exist: {resources / name}")

    groups = []
    with tempfile.TemporaryDirectory(
        prefix="aoe-render-runtime-coverage-"
    ) as temporary:
        directory = Path(temporary)
        for name, tick in CASES:
            groups.append(
                run_case(executable, resources / name, tick, directory)
            )

    events = merge_events(groups)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(
            {
                "schema": "aoe-runtime-render-fallback-v1",
                "events": events,
            },
            separators=(",", ":"),
        )
        + "\n"
    )
    print(
        f"runtime renderer coverage: {len(CASES)} cases, "
        f"{len(events)} deduplicated fallback states"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
