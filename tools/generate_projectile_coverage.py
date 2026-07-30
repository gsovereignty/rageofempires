#!/usr/bin/env python3
"""Emit renderer coverage audit for represented projectile/impact visuals."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


PROJECTILES = [
    ("fire_stream", 3822, 4193, "exact_static", "exact_animated"),
    ("cannonball", 3382, 3803, "body_only", "exact_composite"),
    ("gunpowder_shot", 3396, 4500, "body_only", "exact_composite"),
    ("onager_primary", 3396, 4500, "wrong_variant", "exact_composite"),
    ("onager_volley", 3385, 3986, "frame_zero_only", "exact_composite"),
    ("trebuchet_stone", 3394, 3815, "frame_zero_only", "exact_composite"),
    ("throwing_axe", 3380, 3801, "body_only", "exact_composite"),
    ("arrow", 638, 50, "wrong_frame_zero", "exact_directional"),
    ("scorpion_bolt", 3391, 3812, "wrong_frame_zero", "exact_composite"),
]

IMPACTS = [
    ("standard_explosion", 1744, 416, 10),
    ("fire_gunpowder_explosion", 5463, 4370, 20),
]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/projectile_impact_coverage.json"),
    )
    args = parser.parse_args()
    report = {
        "projectiles": [
            {
                "kind": kind,
                "root_graphic": graphic,
                "slp_id": slp,
                "before": before,
                "after": after,
            }
            for kind, graphic, slp, before, after in PROJECTILES
        ],
        "impacts": [
            {
                "kind": kind,
                "graphic": graphic,
                "slp_id": slp,
                "frame_count": frames,
                "before": "archive_frame_zero_only",
                "after": "exact_animated",
            }
            for kind, graphic, slp, frames in IMPACTS
        ],
        "summary": {
            "projectile_exact_before": 1,
            "projectile_exact_after": 9,
            "projectile_total": 9,
            "impact_exact_before": 0,
            "impact_exact_after": 2,
            "impact_total": 2,
            "fail_closed": [],
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
