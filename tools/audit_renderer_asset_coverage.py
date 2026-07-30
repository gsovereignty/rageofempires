#!/usr/bin/env python3
"""Deterministically inventory represented kinds covered by legacy renderer maps."""

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path


DIRECT_UNITS = {
    "villager", "sheep", "deer", "boar", "relic", "trade_cart",
    "fishing_ship",
}
DIRECT_BUILDINGS = {
    "town_center", "archery_range", "house", "lumber_camp", "mining_camp",
    "blacksmith", "university", "market", "fish_trap", "bombard_tower",
    "outpost", "wonder", "stone_wall", "palisade_wall",
}
UNIQUE_UNITS = {
    "longbowman", "elite_longbowman", "throwing_axeman",
    "elite_throwing_axeman", "huskarl", "elite_huskarl", "teutonic_knight",
    "elite_teutonic_knight", "samurai", "elite_samurai", "chu_ko_nu",
    "elite_chu_ko_nu", "cataphract", "elite_cataphract", "war_elephant",
    "elite_war_elephant", "mameluke", "elite_mameluke", "janissary",
    "elite_janissary", "berserk", "elite_berserk", "mangudai",
    "elite_mangudai", "jaguar_warrior", "elite_jaguar_warrior",
    "plumed_archer", "elite_plumed_archer", "conquistador",
    "elite_conquistador", "tarkan", "elite_tarkan",
}
NAVAL_UNITS = {
    "fishing_ship", "galley", "war_galley", "galleon", "transport_ship",
    "fire_ship", "fast_fire_ship", "demolition_ship",
    "heavy_demolition_ship", "cannon_galleon", "elite_cannon_galleon",
    "longboat", "elite_longboat", "turtle_ship", "elite_turtle_ship",
    "trade_cog",
}


def enum_members(source: str, enum: str) -> list[str]:
    match = re.search(rf"enum class {enum}\s*\{{(.*?)\}};", source, re.S)
    if not match:
        raise ValueError(f"missing enum {enum}")
    return [
        token
        for token in re.findall(r"\b([a-z][a-z0-9_]*)\b", match.group(1))
    ]


def function_body(source: str, signature: str) -> str:
    """Return one C++ function body using balanced braces."""
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise ValueError(f"unterminated function {signature}")


def loader_body(renderer: str) -> str:
    return function_body(renderer, "LegacySprites load_local_legacy_sprites")


def mapped_kinds(renderer: str, kind: str) -> set[str]:
    """Kinds with mapping evidence inside sprite loader, not mere file mentions."""
    body = re.sub(r"//.*?$|/\*.*?\*/", "", loader_body(renderer), flags=re.M | re.S)
    result: set[str] = set()
    for line in body.splitlines():
        if f"{kind}::" not in line:
            continue
        # Comparisons only choose composite policy. Standalone expressions are
        # mentions, not sprite writes. Calls, mapping records, and loop lists
        # retain punctuation proving participation in loader data flow.
        if "==" in line or "!=" in line:
            continue
        for name, tail in re.findall(
            rf"{kind}::([a-z0-9_]+)([^A-Za-z0-9_]*)", line
        ):
            if any(mark in tail for mark in (",", "}", ")")):
                result.add(name)
    return result


def drs_ids(path: Path, extension: bytes = b" pls") -> set[int]:
    data = path.read_bytes()
    if len(data) < 64:
        raise ValueError("truncated DRS")
    table_count = struct.unpack_from("<I", data, 56)[0]
    ids: set[int] = set()
    for index in range(table_count):
        table = 64 + index * 12
        ext = data[table:table + 4]
        offset, count = struct.unpack_from("<II", data, table + 4)
        if ext != extension:
            continue
        for entry in range(count):
            record = offset + entry * 12
            resource, payload, size = struct.unpack_from("<III", data, record)
            if payload + size > len(data):
                raise ValueError("DRS entry outside archive")
            ids.add(resource)
    return ids


def graphic_slps(dat: dict) -> dict[int, int]:
    return {
        int(value["id"]): int(value["slp_id"])
        for value in dat["graphics"]
        if value.get("slp_id") is not None and int(value["slp_id"]) >= 0
    }


def direct_renderer_slps(renderer: str) -> set[int]:
    body = loader_body(renderer)
    ids = {
        int(value)
        for value in re.findall(
            r"\battempt(?:_animation)?\s*\(\s*sprites\.[^,]+,\s*(\d+)",
            body,
            re.S,
        )
    }
    ids.update(
        int(value)
        for value in re.findall(
            r"(?:MilitaryMapping|DeathMapping)\s*\{\s*"
            r"UnitKind::[a-z0-9_]+\s*,\s*(\d+)",
            body,
        )
    )
    for move, attack in re.findall(
        r"ActionMapping\s*\{\s*UnitKind::[a-z0-9_]+\s*,\s*(\d+)\s*,"
        r"\s*\d+\s*,\s*(\d+)",
        body,
    ):
        ids.update((int(move), int(attack)))
    for block in re.findall(
        r"\b[a-z0-9_]+_slps\s*\{\{?(.*?)\}\}?;", body, re.S
    ):
        ids.update(int(value) for value in re.findall(r"\b\d+\b", block))
    return ids


def direct_slps_by_kind(renderer: str) -> dict[tuple[str, str], set[int]]:
    result: dict[tuple[str, str], set[int]] = {}
    for kind, slp in re.findall(
        r"(?:MilitaryMapping|DeathMapping)\s*\{\s*"
        r"UnitKind::([a-z0-9_]+)\s*,\s*(\d+)",
        renderer,
    ):
        result.setdefault(("unit", kind), set()).add(int(slp))
    for kind, move, attack in re.findall(
        r"ActionMapping\s*\{\s*UnitKind::([a-z0-9_]+)\s*,\s*(\d+)\s*,"
        r"\s*\d+\s*,\s*(\d+)",
        renderer,
    ):
        result.setdefault(("unit", kind), set()).update(
            (int(move), int(attack))
        )
    for name in (
        "town_center_slps",
        "town_center_age_slps",
        "town_center_layer_slps",
    ):
        match = re.search(rf"\b{name}\s*\{{\{{?(.*?)\}}\}}?;", renderer, re.S)
        if match:
            result.setdefault(("building", "town_center"), set()).update(
                int(value) for value in re.findall(r"\b\d+\b", match.group(1))
            )
    return result


def classify(unit: str) -> str:
    if unit in NAVAL_UNITS:
        return "naval"
    if unit in UNIQUE_UNITS:
        return "unique"
    return "common"


def build_report(types: str, renderer: str, dat: dict, archive_ids: set[int]) -> dict:
    units = enum_members(types, "UnitKind")
    buildings = enum_members(types, "BuildingKind")
    mapped_units = mapped_kinds(renderer, "UnitKind") | DIRECT_UNITS
    mapped_buildings = mapped_kinds(renderer, "BuildingKind") | DIRECT_BUILDINGS
    graphics = graphic_slps(dat)
    missing_dat_slps = sorted(set(graphics.values()) - archive_ids)
    direct_slps = direct_renderer_slps(renderer)
    by_kind = direct_slps_by_kind(renderer)
    groups = {
        "common": [], "unique": [], "naval": [], "building": [],
    }
    for unit in units:
        absent = sorted(by_kind.get(("unit", unit), set()) - archive_ids)
        groups[classify(unit)].append({
            "kind": unit,
            "renderer_mapping": unit in mapped_units,
            "absent_direct_slps": absent,
            "status": (
                "mapped_asset_gap" if absent
                else "mapped" if unit in mapped_units
                else "guaranteed_fallback"
            ),
        })
    for building in buildings:
        absent = sorted(
            by_kind.get(("building", building), set()) - archive_ids
        )
        groups["building"].append({
            "kind": building,
            "renderer_mapping": building in mapped_buildings,
            "absent_direct_slps": absent,
            "status": (
                "mapped_asset_gap" if absent
                else "mapped" if building in mapped_buildings
                else "guaranteed_fallback"
            ),
        })
    return {
        "schema": "aoe-renderer-asset-coverage-v1",
        "represented": {
            "units": len(units),
            "buildings": len(buildings),
        },
        "renderer": {
            "mapped_units": sum(
                item["renderer_mapping"]
                for name in ("common", "unique", "naval")
                for item in groups[name]
            ),
            "mapped_buildings": sum(
                item["renderer_mapping"] for item in groups["building"]
            ),
            "mapping_evidence": (
                "reachable sprite assignment/load data inside "
                "load_local_legacy_sprites; enum mentions elsewhere excluded"
            ),
        },
        "live_evidence": {
            "dat_graphics": len(dat["graphics"]),
            "graphics_drs_slps": len(archive_ids),
            "dat_linked_slps_absent_from_graphics_drs": missing_dat_slps,
            "renderer_direct_slps": sorted(direct_slps),
            "renderer_direct_slps_absent_from_graphics_drs": sorted(
                direct_slps - archive_ids
            ),
        },
        "groups": groups,
    }


def refresh_from_baseline(
    types: str, renderer: str, baseline: dict
) -> dict:
    """Refresh live source mappings while retaining audited DAT/DRS evidence."""
    live = baseline["live_evidence"]
    absent = set(live["dat_linked_slps_absent_from_graphics_drs"])
    direct = direct_renderer_slps(renderer)
    # build_report only needs archive membership for current direct SLPs. DAT
    # absence and archive counts remain authoritative baseline observations.
    archive_direct = direct - absent
    synthetic_dat = {
        "graphics": [
            {"id": index, "slp_id": slp}
            for index, slp in enumerate(sorted(absent))
        ]
    }
    report = build_report(types, renderer, synthetic_dat, archive_direct)
    report["live_evidence"]["dat_graphics"] = live["dat_graphics"]
    report["live_evidence"]["graphics_drs_slps"] = live["graphics_drs_slps"]
    report["live_evidence"][
        "dat_linked_slps_absent_from_graphics_drs"
    ] = sorted(absent)
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--types", type=Path, default=Path("include/aoe/types.hpp"))
    parser.add_argument("--renderer", type=Path, default=Path("src/sdl_app.cpp"))
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--dat-json", type=Path)
    source.add_argument(
        "--baseline-report",
        type=Path,
        help="retain its audited DAT/DRS evidence and refresh source mappings",
    )
    parser.add_argument("--graphics-drs", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    types = args.types.read_text()
    renderer = args.renderer.read_text()
    if args.baseline_report:
        if args.graphics_drs:
            parser.error("--graphics-drs cannot accompany --baseline-report")
        report = refresh_from_baseline(
            types, renderer, json.loads(args.baseline_report.read_text())
        )
    else:
        if not args.graphics_drs:
            parser.error("--graphics-drs is required with --dat-json")
        report = build_report(
            types,
            renderer,
            json.loads(args.dat_json.read_text()),
            drs_ids(args.graphics_drs),
        )
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
