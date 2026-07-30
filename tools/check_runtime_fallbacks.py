#!/usr/bin/env python3
"""Fail when runtime telemetry contains unexpected procedural fallbacks."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


PROCEDURAL_BODY_CALL_SITES = {
    "render_unit:procedural_body",
    "render_unit_death:procedural_body",
}


def unexpected_fallbacks(report: dict[str, Any]) -> list[dict[str, Any]]:
    if report.get("schema") != "aoe-runtime-render-fallback-v1":
        raise ValueError("runtime report has unsupported schema")
    return [
        event
        for event in report.get("events", [])
        if event.get("renderer_call_site") in PROCEDURAL_BODY_CALL_SITES
        and event.get("status") != "intentional_procedural"
    ]


def format_fallback(event: dict[str, Any]) -> str:
    requested = event.get("requested_asset", {})
    asset_ids = ", ".join(
        f"{name}={requested.get(name)!r}"
        for name in ("graphic_id", "slp_id", "shadow_slp_id")
    )
    return (
        f"entity={event.get('entity_id')} "
        f"tick={event.get('simulation_tick')} "
        f"state={event.get('stable_render_state_key')!r} "
        f"status={event.get('status')!r} "
        f"call_site={event.get('renderer_call_site')!r} "
        f"assets=({asset_ids}) "
        f"reason={event.get('reason')!r}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    arguments = parser.parse_args()
    report = json.loads(arguments.report.read_text())
    try:
        fallbacks = unexpected_fallbacks(report)
    except ValueError as error:
        print(error)
        return 2
    if fallbacks:
        print(
            f"unexpected procedural unit fallbacks: {len(fallbacks)}"
        )
        for event in fallbacks:
            print(format_fallback(event))
        return 1
    print("runtime procedural unit fallback check accepted")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
