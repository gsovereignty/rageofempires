#!/usr/bin/env python3
"""Inventory all represented kinds against guarded DAT-shadow renderer paths."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def enum_members(source: str, enum: str) -> list[str]:
    match = re.search(rf"enum class {enum}\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"missing enum {enum}")
    return re.findall(r"\b([a-z][a-z0-9_]*)\b", match.group(1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--types", type=Path, default=Path("include/aoe/types.hpp"))
    parser.add_argument(
        "--coverage",
        type=Path,
        default=Path("generated/renderer_asset_coverage.json"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/shadow_binding_catalog.json"),
    )
    args = parser.parse_args()
    source = args.types.read_text()
    coverage = json.loads(args.coverage.read_text())
    units = enum_members(source, "UnitKind")
    buildings = enum_members(source, "BuildingKind")
    groups = coverage["groups"]
    status = {
        item["kind"]: item["status"]
        for group in groups.values()
        for item in group
    }
    composite_units = {
        "galley", "war_galley", "galleon", "transport_ship",
        "fire_ship", "fast_fire_ship", "demolition_ship",
        "heavy_demolition_ship", "cannon_galleon",
        "elite_cannon_galleon", "longboat", "elite_longboat",
        "turtle_ship", "elite_turtle_ship", "trade_cog",
    }
    composite_buildings = {
        "town_center", "barracks", "mill", "archery_range",
        "watch_tower", "stable", "castle", "siege_workshop", "dock",
        "outpost", "monastery", "stone_gate_x", "stone_gate_y",
        "wonder",
    }
    guarded_static_buildings = {
        "house", "lumber_camp", "mining_camp", "blacksmith",
        "university", "stone_wall", "market", "bombard_tower",
        "fish_trap",
    }
    guarded_construction_buildings = {
        "wonder", "outpost", "fish_trap", "bombard_tower",
    }
    existing_death_buildings = {"wonder", "outpost"}
    guarded_death_buildings = {"bombard_tower"}

    def building_state(
        kind: str, state: str, standing_path: str
    ) -> dict[str, str]:
        if state in {"standing", "damage"}:
            path = standing_path
            before = (
                "exact_DAT_shadow"
                if path in {"dat_static_composite", "explicit_exact_shadow_slp"}
                else "procedural_fallback"
            )
            after = (
                "guarded_exact_DAT_shadow"
                if path == "guarded_unique_layer10_runtime_binding"
                else before
            )
            note = (
                "standing shadow reused; damage flames remain procedural"
                if state == "damage"
                else "standing body/shadow path"
            )
        elif state == "construction":
            path = (
                "guarded_unique_layer10_runtime_binding"
                if kind in guarded_construction_buildings
                else "procedural_fallback"
            )
            before = "procedural_fallback"
            after = (
                "guarded_exact_DAT_shadow"
                if kind in guarded_construction_buildings
                else before
            )
            note = "construction animation/body path"
        else:
            path = (
                "existing_DAT_animation_shadow"
                if kind in existing_death_buildings
                else (
                    "guarded_unique_layer10_runtime_binding"
                    if kind in guarded_death_buildings
                    else "procedural_fallback"
                )
            )
            before = (
                "exact_DAT_shadow"
                if kind in existing_death_buildings
                else "procedural_fallback"
            )
            after = (
                "guarded_exact_DAT_shadow"
                if kind in guarded_death_buildings
                else before
            )
            note = "death/rubble renderer path"
        return {
            "before": before,
            "after": after,
            "renderer_path": path,
            "qualification": note,
        }

    def row(kind: str, category: str) -> dict:
        mapped = status.get(kind, "guaranteed_fallback") != "guaranteed_fallback"
        if category == "unit" and kind in composite_units:
            path = "dat_animated_composite"
        elif category == "building" and kind in composite_buildings:
            path = "dat_static_composite"
        elif kind == "palisade_wall":
            path = "explicit_exact_shadow_slp"
        elif mapped and category == "unit":
            path = "guarded_root_slp_binding"
        elif category == "building" and kind in guarded_static_buildings:
            path = "guarded_unique_layer10_runtime_binding"
        elif mapped:
            path = "procedural_fallback"
        else:
            path = "procedural_fallback"
        if category == "building":
            states = {
                state: building_state(kind, state, path)
                for state in ("standing", "construction", "damage", "death")
            }
        else:
            states = {
                state: (
                    "guarded_by_exact_DAT_layer"
                    if path in {
                        "dat_animated_composite",
                        "guarded_root_slp_binding",
                    }
                    else "procedural_or_unmapped"
                )
                for state in ("standing", "moving", "attack", "death")
            }
        return {
            "kind": kind,
            "category": category,
            "renderer_asset_status": status.get(
                kind, "guaranteed_fallback"
            ),
            "shadow_path": path,
            "live_binding": (
                "resolved_at_runtime_from user DAT/DRS"
                if path in {
                    "guarded_root_slp_binding",
                    "guarded_unique_layer10_runtime_binding",
                }
                else "renderer source path"
            ),
            "states": states,
        }

    report = {
        "schema": "aoe-shadow-binding-catalog-v2",
        "represented": {"units": len(units), "buildings": len(buildings)},
        "building_state_coverage": {
            "standing": {"before": 15, "after": 24, "total": 27},
            "construction": {"before": 0, "after": 4, "total": 27},
            "damage_shadow": {"before": 15, "after": 24, "total": 27},
            "death": {"before": 2, "after": 3, "total": 27},
        },
        "evidence": {
            "dat": (
                "LegacyGraphic root deltas, layer, player_color, frame_count, "
                "angle_count, offsets, display_angle"
            ),
            "archive": (
                "SLP presence is checked by guarded runtime decode; missing "
                "payload retains procedural fallback"
            ),
            "baseline": str(args.coverage),
        },
        "policy": {
            "exact_direct_binding": (
                "all graphics sharing root SLP must agree on exactly one "
                "direct layer-10, player_color=-1 child; shadow frames must "
                "equal root frames or be static"
            ),
            "ambiguity": "fallback",
            "guarded_meaning": (
                "candidate is promoted only when live DAT has one exact "
                "neutral direct layer-10 child and live DRS contains its SLP"
            ),
        },
        "entities": (
            [row(kind, "unit") for kind in units]
            + [row(kind, "building") for kind in buildings]
        ),
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
