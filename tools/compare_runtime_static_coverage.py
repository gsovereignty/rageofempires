#!/usr/bin/env python3
"""Require runtime fallback telemetry to match static coverage evidence."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def building_key(
    object_kind: str, state: dict[str, Any]
) -> tuple[Any, ...]:
    return (
        object_kind,
        state.get("building_state"),
        state.get("owner"),
        state.get("age"),
        state.get("civilization"),
        state.get("architecture_family"),
        state.get("damage_stage", 0),
        state.get("construction_stage", 0),
        state.get("topology_frame", state.get("animation_frame", 0)),
        state.get("upgrade_variant", 0),
    )


def unit_key(
    object_kind: str, state: dict[str, Any]
) -> tuple[Any, ...]:
    return (
        object_kind,
        state.get("action"),
        state.get("action_detail", "none"),
        state.get("moving", False),
        state.get("owner"),
        state.get("architecture_family"),
        state.get("direction"),
        state.get("animation_frame", 0),
    )


def projectile_key(
    object_kind: str, state: dict[str, Any]
) -> tuple[Any, ...]:
    category = state.get("object_category", state.get("category"))
    if object_kind in {"generic_splash_stone", "generic_impact"}:
        return (object_kind, category)
    return (
        object_kind,
        category,
        state.get("shadow", False),
        state.get("direction", 0),
        state.get("animation_frame", 0),
    )


def compare(
    static_report: dict[str, Any],
    runtime_report: dict[str, Any],
) -> list[str]:
    if static_report.get("schema") != "aoe-renderer-asset-coverage-v2":
        return ["static report has unsupported schema"]
    if runtime_report.get("schema") != "aoe-runtime-render-fallback-v1":
        return ["runtime report has unsupported schema"]

    static_buildings: dict[tuple[Any, ...], dict[str, Any]] = {}
    static_units: dict[tuple[Any, ...], dict[str, Any]] = {}
    static_projectiles: dict[tuple[Any, ...], dict[str, Any]] = {}
    for row in static_report.get("rows", []):
        dimensions = row.get("state_dimensions", {})
        if "building_state" in dimensions:
            key = building_key(row.get("object_kind", ""), dimensions)
            if key in static_buildings:
                return [f"static report has duplicate state {key!r}"]
            static_buildings[key] = row
            continue
        if "action" in dimensions:
            key = unit_key(row.get("object_kind", ""), dimensions)
            if key in static_units:
                return [f"static report has duplicate unit state {key!r}"]
            static_units[key] = row
            continue
        if "object_category" in dimensions:
            key = projectile_key(row.get("object_kind", ""), dimensions)
            if key in static_projectiles:
                return [
                    f"static report has duplicate projectile state {key!r}"
                ]
            static_projectiles[key] = row

    problems: list[str] = []
    for event in runtime_report.get("events", []):
        state = event.get("render_state", {})
        category = state.get("category")
        if category in {"building", "building_rubble"}:
            key = building_key(state.get("object_kind", ""), state)
            row = static_buildings.get(key)
            label = "building"
        elif category in {"unit", "unit_death"}:
            key = unit_key(state.get("object_kind", ""), state)
            row = static_units.get(key)
            label = "unit"
        elif category in {"projectile", "impact", "resource"}:
            key = projectile_key(state.get("object_kind", ""), state)
            row = static_projectiles.get(key)
            label = category
        else:
            continue
        if row is None:
            problems.append(
                f"runtime {label} state absent from static audit: {key!r}"
            )
            continue
        if row.get("status") != event.get("status"):
            problems.append(
                f"status mismatch for {key!r}: "
                f"static={row.get('status')!r} "
                f"runtime={event.get('status')!r}"
            )
        expected = row.get("expected_asset_ids", {})
        requested = event.get("requested_asset", {})
        for field in (
            "graphic_id",
            "slp_id",
            "shadow_slp_id",
            "overlay_graphic_ids",
        ):
            if expected.get(field) != requested.get(field):
                problems.append(
                    f"{field} mismatch for {key!r}: "
                    f"static={expected.get(field)!r} "
                    f"runtime={requested.get(field)!r}"
                )
        static_reason = row.get("failure_reason", "")
        runtime_reason = event.get("reason", "")
        if static_reason and static_reason != runtime_reason:
            problems.append(
                f"failure reason mismatch for {key!r}: "
                f"static={static_reason!r} runtime={runtime_reason!r}"
            )
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("static_report", type=Path)
    parser.add_argument("runtime_report", type=Path)
    arguments = parser.parse_args()
    static_report = json.loads(arguments.static_report.read_text())
    runtime_report = json.loads(arguments.runtime_report.read_text())
    problems = compare(static_report, runtime_report)
    if problems:
        for problem in problems:
            print(problem)
        return 1
    print(
        "runtime/static coverage agreement accepted: "
        f"{len(runtime_report.get('events', []))} runtime events"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
