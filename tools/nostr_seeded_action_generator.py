#!/usr/bin/env python3
"""Deterministic coverage-priority planning and retained replay prefixes."""

from __future__ import annotations

import argparse
import hashlib
import random

from nostr_visual_coverage import cell_key, required_cells


GENERATOR_VERSION = "coverage-priority-v1"


def coverage_priority_directions(
    plan: dict[str, object], owner: int, seed: int,
) -> list[int]:
    """Order 8-way commands by uncovered cells, with seeded cyclic fallback."""
    prioritized: list[int] = []
    for cell in plan.get("cells", []):
        if int(cell.get("owner", -1)) != owner or \
                int(cell.get("directionCount", 0)) != 8:
            continue
        direction = int(cell.get("logicalDirection", -1))
        if 0 <= direction < 8 and direction not in prioritized:
            prioritized.append(direction)
    rng = random.Random(seed ^ (owner * 0x9E3779B97F4A7C15))
    start = prioritized[0] if prioritized else rng.randrange(8)
    step = -1 if rng.randrange(2) else 1
    return [(start + step * offset) % 8 for offset in range(8)]


def coverage_priority_plan(
    specification: dict[str, object],
    observed_records: list[dict[str, object]],
    seed: int,
) -> dict[str, object]:
    counts: dict[str, int] = {}
    for record in observed_records:
        normalized = {
            "peer": record.get("peer"), "owner": record.get("owner"),
            "unitKind": record.get("unitKind"), "action": record.get("action"),
            "directionCount": record.get("directionCount"),
            "logicalDirection": record.get("logicalDirection"),
            "mirrored": int(record.get("mirroringMode", 0)) != 0,
            "transitionKind": record.get(
                "transitionKind", "authoritative-step"
            ),
        }
        key = cell_key(normalized)
        counts[key] = counts.get(key, 0) + 1
    minimum = int(specification["minimumSamplesPerCell"])
    cells = required_cells(specification)
    rng = random.Random(seed)
    decorated = [
        (counts.get(cell_key(cell), 0), rng.random(), cell)
        for cell in cells
        if counts.get(cell_key(cell), 0) < minimum
    ]
    decorated.sort(key=lambda value: (value[0], value[1], cell_key(value[2])))
    owner_order: list[int] = []
    for _, _, cell in decorated:
        owner = int(cell["owner"])
        if owner not in owner_order:
            owner_order.append(owner)
    return {
        "schemaVersion": 1,
        "generatorVersion": GENERATOR_VERSION,
        "seed": seed,
        "minimumSamplesPerCell": minimum,
        "uncoveredCellCount": len(decorated),
        "ownerOrder": owner_order,
        "cells": [cell for _, _, cell in decorated],
    }


def rotating_seed(source_commit: str) -> int:
    digest = hashlib.sha256(source_commit.encode("ascii")).digest()
    return int.from_bytes(digest[:8], "big")


def causal_replay_prefix(
    actions: list[dict[str, object]], failure: dict[str, object]
) -> dict[str, object]:
    """Drop every action after first failure evidence capture boundary."""
    completed = failure.get("completedEvidence") or {}
    retained = completed.get("actions") if isinstance(completed, dict) else None
    source = retained if isinstance(retained, list) else actions
    failure_tick_candidates = [
        int(value)
        for value in (
            (failure.get("hostRender") or {}).get("tick")
            if isinstance(failure.get("hostRender"), dict) else None,
            (failure.get("joinRender") or {}).get("tick")
            if isinstance(failure.get("joinRender"), dict) else None,
        ) if value is not None
    ]
    failure_tick = min(failure_tick_candidates) \
        if failure_tick_candidates else None
    if failure_tick is None:
        prefix = list(source)
    else:
        prefix = [
            action for action in source
            if int(action.get("telemetryTick", failure_tick)) <= failure_tick
        ]
    return {
        "schemaVersion": 1,
        "kind": "causal-action-prefix",
        "failure": failure.get("error"),
        "failureTick": failure_tick,
        "originalActionCount": len(actions),
        "retainedActionCount": len(prefix),
        "actions": prefix,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--commit", required=True)
    arguments = parser.parse_args()
    print(rotating_seed(arguments.commit))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
